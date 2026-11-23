// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuroraSky/AuroraSkyShaderDispatch.h"

#include "AuroraSky/AuroraSkyShaders.h"
#include "GlobalShader.h"
#include "RenderGraphUtils.h"
#include "RHI.h"
#include "RHICommandList.h"

void StarSky_DispatchAuroraSky(
	FRHICommandList& RHICmdList,
	FRHITexture* CubeTexture,
	FRHIUnorderedAccessView* CubeUAV,
	int32 CubeSize,
	int32 FirstFace,
	int32 NumFaces,
	float SkyTime)
{
	if (!CubeTexture || !CubeUAV || CubeSize <= 0 || NumFaces <= 0)
	{
		return;
	}

	// 夹到合法范围，避免调用方传错导致越界写
	FirstFace = FMath::Clamp(FirstFace, 0, 5);
	NumFaces = FMath::Clamp(NumFaces, 1, 6 - FirstFace);

	FAuroraSkyCS::FParameters ShaderParams;
	ShaderParams.RWOutput = CubeUAV;
	ShaderParams.CubeSize = static_cast<uint32>(CubeSize);
	ShaderParams.CubeFaceBase = static_cast<uint32>(FirstFace);
	ShaderParams.SkyTime = SkyTime;

	TShaderMapRef<FAuroraSkyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	const int32 Groups = FMath::DivideAndRoundUp(CubeSize, 8);
	const FIntVector GroupCount(Groups, Groups, NumFaces);

	// 上一帧这张 cube 是当 SRV 被天空材质采样的，写之前必须先切到 UAV，写完再切回来
	RHICmdList.Transition(FRHITransitionInfo(CubeTexture, ERHIAccess::SRVMask, ERHIAccess::UAVCompute));
	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, ShaderParams, GroupCount);
	RHICmdList.Transition(FRHITransitionInfo(CubeTexture, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
}

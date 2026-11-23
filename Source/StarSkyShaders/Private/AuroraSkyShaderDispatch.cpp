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
	float SkyTime)
{
	if (!CubeTexture || !CubeUAV || CubeSize <= 0)
	{
		return;
	}

	FAuroraSkyCS::FParameters ShaderParams;
	ShaderParams.RWOutput = CubeUAV;
	ShaderParams.CubeSize = static_cast<uint32>(CubeSize);
	ShaderParams.SkyTime = SkyTime;

	TShaderMapRef<FAuroraSkyCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	const int32 Groups = FMath::DivideAndRoundUp(CubeSize, 8);
	const FIntVector GroupCount(Groups, Groups, 6);

	// 上一帧这张 cube 是当 SRV 被天空材质采样的，写之前必须先切到 UAV，写完再切回来
	RHICmdList.Transition(FRHITransitionInfo(CubeTexture, ERHIAccess::SRVMask, ERHIAccess::UAVCompute));
	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, ShaderParams, GroupCount);
	RHICmdList.Transition(FRHITransitionInfo(CubeTexture, ERHIAccess::UAVCompute, ERHIAccess::SRVMask));
}

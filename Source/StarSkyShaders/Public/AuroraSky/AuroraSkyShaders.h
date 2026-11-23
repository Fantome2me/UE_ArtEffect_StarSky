// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"

/**
 * 全局计算着色器 FAuroraSkyCS —— 跑完整的 StarNest 星云 + 星星 + 极光。
 *
 * 一个 thread 计算 cube 的一个 texel；cube 以 Texture2DArray 方式绑定 UAV，
 * 一次 dispatch 覆盖全部 6 个面（Z 维 = face index）。
 *
 * 天空参数（Volsteps / Iterations / 颜色 / 变暗量 …）全部是 AuroraSky.usf 里的
 * **编译期常量**，所以这里只剩下 3 个 uniform。
 *
 * 所属模块 StarSkyShaders 的 LoadingPhase 必须是 PostConfigInit。
 */
class FAuroraSkyCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FAuroraSkyCS);
	SHADER_USE_PARAMETER_STRUCT(FAuroraSkyCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWTexture2DArray<float4>, RWOutput)
		SHADER_PARAMETER(uint32, CubeSize)
		SHADER_PARAMETER(float, SkyTime)
	END_SHADER_PARAMETER_STRUCT()

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

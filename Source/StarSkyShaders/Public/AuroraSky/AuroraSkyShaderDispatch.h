// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandList;
class FRHITexture;
class FRHIUnorderedAccessView;

/**
 * 在渲染线程把星空烘到 cube 贴图的全部 6 个面上（一次 dispatch）。
 *
 * 天空参数全部固化在 AuroraSky.usf 里，运行时只传三项：单面边长、演化时间。
 *
 * 函数由 StarSkyShaders 模块导出，调用方完全不需要接触 FGlobalShader 类型，
 * 因此避开跨模块导出 shader 类型的链接问题。
 *
 * @param CubeTexture  cube RT 的 RHI 贴图（会被在 SRV / UAV 间切屏障）
 * @param CubeUAV      覆盖全部 6 个 slice 的 UAV
 * @param CubeSize     单面边长（texel）
 * @param SkyTime      天空演化时间（秒）
 */
STARSKYSHADERS_API void StarSky_DispatchAuroraSky(
	FRHICommandList& RHICmdList,
	FRHITexture* CubeTexture,
	FRHIUnorderedAccessView* CubeUAV,
	int32 CubeSize,
	float SkyTime);

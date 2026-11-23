// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandList;
class FRHITexture;
class FRHIUnorderedAccessView;

/**
 * 在渲染线程把整片星空烘到一张 cube 贴图上（6 个面一次 dispatch）。
 *
 * 天空参数全部固化在 AuroraSky.usf 里，运行时只传两项：
 * cube 单面边长（texel）与天空演化时间（秒）。
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

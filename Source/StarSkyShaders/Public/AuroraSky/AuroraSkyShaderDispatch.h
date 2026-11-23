// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FRHICommandList;
class FRHITexture;
class FRHIUnorderedAccessView;

/**
 * 在渲染线程把星空烘到 cube 贴图的一段面上。
 *
 * 天空参数全部固化在 AuroraSky.usf 里，运行时只传四项：
 * 单面边长、起始面、面数、天空演化时间。
 *
 * **按面调度**：把原来的「6 面一次 dispatch」拆成「每次 1~N 个面」。
 * 调用方可以每帧只烘 1 个面，从而把单帧的 GPU 峰值压到 1/6，
 * 显著减小帧率卡顿感（代价是每个面的刷新间隔变长）。
 *
 * 函数由 StarSkyShaders 模块导出，调用方完全不需要接触 FGlobalShader 类型，
 * 因此避开跨模块导出 shader 类型的链接问题。
 *
 * @param CubeTexture  cube RT 的 RHI 贴图（会被在 SRV / UAV 间切屏障）
 * @param CubeUAV      覆盖全部 6 个 slice 的 UAV
 * @param CubeSize     单面边长（texel）
 * @param FirstFace    本次 dispatch 的起始面（0..5）
 * @param NumFaces     本次 dispatch 的面数（1..6，且 FirstFace + NumFaces <= 6）
 * @param SkyTime      天空演化时间（秒）
 */
STARSKYSHADERS_API void StarSky_DispatchAuroraSky(
	FRHICommandList& RHICmdList,
	FRHITexture* CubeTexture,
	FRHIUnorderedAccessView* CubeUAV,
	int32 CubeSize,
	int32 FirstFace,
	int32 NumFaces,
	float SkyTime);

// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AuroraSkySettings.generated.h"

class UMaterialInterface;

/**
 * 星空的运行时配置。
 *
 * 注意：**星云/极光/星星的画面参数已经全部固化在 AuroraSky.usf 的编译期常量里**
 * （见方案文档 v2 §3.1 / §3.2 的「材质实际」列），这里不再暴露它们 ——
 * 那样做可以把 uniform 数量降到最低，循环上界变成字面量，显著降低每个 texel 的开销。
 *
 * 想改观感就去改 `Project/Shaders/Private/AuroraSky.usf` 顶部的 #define 与 static const，
 * 改完重启编辑器即可（只需重编着色器，不用重编 C++）。
 *
 * 这里保留的都是**运行时确实需要**的项，重点是画质 / 平滑度的权衡。
 */
USTRUCT(BlueprintType)
struct FAuroraSkySettings
{
	GENERATED_BODY()

	/** 天空材质（必须 Unlit + Opaque + bIsSky，采样名为 AuroraSkyCube 的 Cube 贴图） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "渲染")
	TObjectPtr<UMaterialInterface> SkyMaterial = nullptr;

	/**
	 * 烘焙用 Cube 单面分辨率。
	 * 1024² 时点状恒星是锐利的圆点；512 已经够用；256 会糊成方块。
	 * 显存占用 = 面数 × 分辨率² × 8 字节（RGBA16F）：1024 → 约 50 MB。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "渲染", meta = (ClampMin = "32", ClampMax = "2048"))
	int32 CubeResolution = 1024;

	/**
	 * 烘焙频率（Hz）。
	 * 每次烘焙 = 一次 dispatch 算完全部 6 个面。
	 * 注意成本随分辨率呈平方增长：1024² × 6 × 15 Hz 已经不轻，
	 * 想降开销优先调这里或降分辨率。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "渲染", meta = (ClampMin = "1", ClampMax = "240"))
	float UpdateFrequency = 15.0f;

	/** 最终天空亮度倍率（材质参数 SkyIntensity），用来和 UE 的曝光/色调映射对齐。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "渲染", meta = (ClampMin = "0.0"))
	float SkyIntensity = 1.0f;

	/** 出生时是否在没有穹顶的世界里自动补一个 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "渲染")
	bool bAutoSpawnDome = true;

	/** 天空演化时间倍率（0 = 暂停） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "全局")
	float SkyTimeScale = 1.0f;

	/** 起始时间偏移（秒），用于和 Unity 对齐某一帧 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "全局")
	float SkyTimeOffset = 0.0f;
};

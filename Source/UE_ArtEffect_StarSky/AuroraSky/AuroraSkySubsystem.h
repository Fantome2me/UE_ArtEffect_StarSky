// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AuroraSky/AuroraSkySettings.h"
#include "AuroraSkySubsystem.generated.h"

class UTextureRenderTargetCube;
class UMaterialInstanceDynamic;
class AAuroraSkyDome;

/**
 * 动态星空总控（方案 2.1 节的数据流）。
 *
 *   SkyTime
 *        ↓
 *   FAuroraSkyCS  (cube，**按面 dispatch**；所有天空参数是编译期常量)
 *        ↓
 *   UTextureRenderTargetCube (PF_FloatRGBA, 带 UAV)
 *        ↓
 *   M_AuroraSky (Unlit + bIsSky) 采样 cube
 *        ↓
 *   穹顶球体 + SkyLight Real Time Capture
 *
 * ## 按面调度（帧率平滑的关键）
 *
 * 原先每次烘焙都是「6 个面一次 dispatch」，单帧会出现一个很大的突刺。
 * 现在改成**每帧最多烘 1 个面**，从「最该刷新」的面里挑：
 *   · 视锥内的可见面 → 目标 `UpdateFrequency`（默认 24 Hz）
 *   · 视锥外的面     → `OffscreenFrequency`（默认 2 Hz）
 * 于是单帧 GPU 峰值降到原来的 1/6，而玩家真正看得到的那几个面仍然刷得很勤。
 * 实际每面刷新率 ≈ min(UpdateFrequency, 帧率 / 可见面数)。
 *
 * 在编辑器世界里也会 tick（IsTickableInEditor），不打 PIE 也能看到动态星空；
 * 但编辑器没有 PlayerController，可见性判定会退化成「6 面全部可见」。
 */
UCLASS()
class UAuroraSkySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	//~ Begin FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bSkyEnabled; }
	virtual bool IsTickableInEditor() const override { return true; }
	//~ End FTickableGameObject

	/** 打开 / 关闭整个星空系统（关闭后停止 tick，不再烘焙） */
	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	void SetSkyEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	bool IsSkyEnabled() const { return bSkyEnabled; }

	/** 立刻把 6 个面全部重算一遍（调试 / 落地脚本用，会一次性花掉完整成本） */
	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	void BakeNow();

	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	AAuroraSkyDome* GetActiveDome() const { return CachedDome; }

	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	int32 GetCubeResolution() const { return CubeResolution; }

	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	UTextureRenderTargetCube* GetSkyCubeRenderTarget() const { return SkyCubeRT; }

	/** 天空已演化的时间（秒） */
	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	float GetSkyTime() const { return SkyTime; }

	/** 当前这一帧判定为「视锥内」的 cube 面位掩码（bit 0..5 = +X,-X,+Y,-Y,+Z,-Z） */
	UFUNCTION(BlueprintCallable, Category = "AuroraSky")
	int32 GetVisibleFaceMask() const;

	static UAuroraSkySubsystem* Get(const UWorld* World);

private:
	void ResolveDome();
	FAuroraSkySettings GetActiveSettings() const;
	void EnsureRenderTarget(int32 Resolution);
	void ReleaseRenderTarget();
	void BindCubeToDome(const FAuroraSkySettings& Settings);
	void DispatchFaces(int32 FirstFace, int32 NumFaces, float InSkyTime);

	/** 选出本帧最该刷新的那个面；返回 INDEX_NONE 表示这帧不需要烘 */
	int32 SelectNextFace(const FAuroraSkySettings& Settings) const;

	/** 当前视图覆盖了哪几个 cube 面（6 bit 掩码）。没有相机时返回全部可见。 */
	int32 ResolveVisibleFaceMask(const FAuroraSkySettings& Settings) const;

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTargetCube> SkyCubeRT;

	UPROPERTY(Transient)
	TObjectPtr<AAuroraSkyDome> CachedDome;

	/** 已经绑到材质上的那张 RT，用来避免每帧重设贴图参数 */
	TWeakObjectPtr<UTexture> LastBoundTexture;

	/** 上次绑定时使用的 MID；MID 若被重建（例如改材质/改设置），贴图参数需要重新绑一次 */
	TWeakObjectPtr<UMaterialInstanceDynamic> LastBoundMID;

	/** RT 刚创建 / 刚重建：需要立刻把 6 个面全烘一遍，避免开头几帧天空是黑的 */
	bool bNeedsFullBake = true;

	bool bSkyEnabled = true;
	bool bLoggedNotReady = false;

	float SkyTime = 0.0f;

	/** 累计调度时钟（只在 Tick 里累加，与 SkyTimeScale 无关） */
	float ScheduleClock = 0.0f;

	/** 每个面上次被烘的时间（ScheduleClock 单位） */
	float FaceLastUpdateTime[6] = { -1000.0f, -1000.0f, -1000.0f, -1000.0f, -1000.0f, -1000.0f };

	int32 CubeResolution = 0;
};

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
 *   FAuroraSkyCS  (cube；所有天空参数是编译期常量)
 *        ↓
 *   UTextureRenderTargetCube (PF_FloatRGBA, 带 UAV)
 *        ↓
 *   M_AuroraSky (Unlit + bIsSky) 采样 cube
 *        ↓
 *   穹顶球体 + SkyLight Real Time Capture
 *
 * 烘焙节奏：按 `UpdateFrequency` 节流，每次烘焙用**一次 dispatch 算完全部 6 个面**。
 * 成本是 `分辨率² × 6 × 频率`，所以在 1024 / 24 Hz 下开销不小 —— 要降就降这两项。
 *
 * 在编辑器世界里也会 tick（IsTickableInEditor），不打 PIE 也能看到动态星空。
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

	/** 立刻烘焙一次（不等节流） */
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

	static UAuroraSkySubsystem* Get(const UWorld* World);

private:
	void ResolveDome();
	FAuroraSkySettings GetActiveSettings() const;
	void EnsureRenderTarget(int32 Resolution);
	void ReleaseRenderTarget();
	void BindCubeToDome(const FAuroraSkySettings& Settings);
	void DispatchBake(float InSkyTime);

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTargetCube> SkyCubeRT;

	UPROPERTY(Transient)
	TObjectPtr<AAuroraSkyDome> CachedDome;

	/** 已经绑到材质上的那张 RT，用来避免每帧重设贴图参数 */
	TWeakObjectPtr<UTexture> LastBoundTexture;

	/** 上次绑定时使用的 MID；MID 若被重建（例如改材质/改设置），贴图参数需要重新绑一次 */
	TWeakObjectPtr<UMaterialInstanceDynamic> LastBoundMID;

	/** RT 刚创建 / 刚重建：下一帧无条件烘一次，避免开头天空是黑的 */
	bool bNeedsBake = true;

	bool bSkyEnabled = true;
	bool bLoggedNotReady = false;

	float SkyTime = 0.0f;

	/** 距下次烘焙还剩多少秒 */
	float TimeUntilNextBake = 0.0f;

	int32 CubeResolution = 0;
};

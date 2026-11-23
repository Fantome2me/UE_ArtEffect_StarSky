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
 *   FAuroraSkyCS  (cube，6 面一次 dispatch；所有天空参数是编译期常量)
 *        ↓
 *   UTextureRenderTargetCube (PF_FloatRGBA, 带 UAV)  ← 10-15 Hz 节流
 *        ↓
 *   M_AuroraSky (Unlit + bIsSky) 采样 cube
 *        ↓
 *   穹顶球体 + SkyLight Real Time Capture
 *
 * 在编辑器世界里也会 tick（IsTickableInEditor），所以不打 PIE 也能在视口里看到动态星空。
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

	/** 跳过节流，立刻重算一次天空 */
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
	void PerformBake();
	void DispatchBake(float InSkyTime);

	UPROPERTY(Transient)
	TObjectPtr<UTextureRenderTargetCube> SkyCubeRT;

	UPROPERTY(Transient)
	TObjectPtr<AAuroraSkyDome> CachedDome;

	/** 已经绑到材质上的那张 RT，用来避免每帧重设贴图参数 */
	TWeakObjectPtr<UTexture> LastBoundTexture;

	bool bSkyEnabled = true;
	bool bLoggedNotReady = false;

	float SkyTime = 0.0f;
	float TimeUntilNextBake = 0.0f;

	int32 CubeResolution = 0;
};

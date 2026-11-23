// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AuroraSky/AuroraSkySettings.h"
#include "AuroraSkyDome.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UTexture;

/**
 * 星空穹顶。
 *
 * 一个反着看的巨大球体，材质 Domain = Sky（走引擎的 SkyPass，天然被场景几何遮挡），
 * 因此它和「天空」完全等价：抬头是星空，低头是地面。
 *
 * 真正的像素是在 UAuroraSkySubsystem 里用计算着色器烘到 Cube RT 上的；
 * 这个 Actor 只负责：
 *   1. 承载 M_AuroraSky 的 MID；
 *   2. 在 Details 面板里暴露全部可调参数（FAuroraSkySettings）；
 *   3. 跟随相机，避免相机飞出穹顶。
 */
UCLASS(BlueprintType, Blueprintable)
class AAuroraSkyDome : public AActor
{
	GENERATED_BODY()

public:
	AAuroraSkyDome();

	/** 默认天空材质路径（Domain = Sky，采样 AuroraSkyCube 这枚 Cube 贴图） */
	static const TCHAR* GetDefaultSkyMaterialPath();

	/** 穹顶球体网格 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "星空")
	TObjectPtr<UStaticMeshComponent> DomeMesh;

	/** 全部作者级参数 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "星空")
	FAuroraSkySettings SkySettings;

	/** 穹顶半径（米）。默认 1 km，足够包住整个关卡。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "星空|穹顶", meta = (ClampMin = "10.0"))
	float DomeRadiusMeters = 1000.0f;

	/** 是否跟随玩家相机移动（避免相机走出穹顶） */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "星空|穹顶")
	bool bFollowCamera = true;

	/** 取得（必要时创建）天空材质的动态实例 */
	UFUNCTION(BlueprintCallable, Category = "星空")
	UMaterialInstanceDynamic* GetSkyMaterialInstance();

	/** 把计算好的 Cube RT 绑到材质参数上 */
	UFUNCTION(BlueprintCallable, Category = "星空")
	void BindSkyCubeTexture(UTexture* InCubeTexture);

	/** 把穹顶缩放到 DomeRadiusMeters */
	UFUNCTION(BlueprintCallable, Category = "星空")
	void UpdateDomeScale();

	//~ Begin AActor Interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor Interface

private:
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SkyMID;

	void ApplySkyMaterial();
};

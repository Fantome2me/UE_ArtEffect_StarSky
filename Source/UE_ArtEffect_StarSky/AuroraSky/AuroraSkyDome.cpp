// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuroraSky/AuroraSkyDome.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UE_ArtEffect_StarSky.h"

namespace
{
	/** /Engine/BasicShapes/Sphere 的半径（单位：uu） */
	constexpr float GSphereBaseRadiusUU = 50.0f;
}

const TCHAR* AAuroraSkyDome::GetDefaultSkyMaterialPath()
{
	return TEXT("/Game/ArtEffect/StarSky/M_AuroraSky.M_AuroraSky");
}

AAuroraSkyDome::AAuroraSkyDome()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = 0.0f;

	DomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	SetRootComponent(DomeMesh);

	DomeMesh->SetMobility(EComponentMobility::Movable);
	DomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DomeMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	DomeMesh->SetGenerateOverlapEvents(false);
	DomeMesh->SetCanEverAffectNavigation(false);
	DomeMesh->SetReceivesDecals(false);

	// 穹顶既不投影也不参与间接光，它只是一个「天空」
	DomeMesh->SetCastShadow(false);
	DomeMesh->bCastDynamicShadow = false;
	DomeMesh->bCastStaticShadow = false;
	DomeMesh->bAffectDistanceFieldLighting = false;
	DomeMesh->bAffectDynamicIndirectLighting = false;

	// 天空需要被 SkyLight 的 Real Time Capture 看到，否则场景不会受星空照亮
	DomeMesh->bVisibleInRealTimeSkyCaptures = true;
	DomeMesh->bVisibleInReflectionCaptures = true;

	// 永远不要在远处被裁掉
	DomeMesh->LDMaxDrawDistance = 0;
	DomeMesh->bNeverDistanceCull = true;

	// 缩放很大，必须放大包围盒，否则会被视锥剔除
	DomeMesh->SetBoundsScale(100.0f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMeshFinder.Succeeded())
	{
		DomeMesh->SetStaticMesh(SphereMeshFinder.Object);
	}

	// 默认材质。首次编译时资产可能还不存在，此时只打印一条警告，等资产建好后重新编译即可。
	{
		static ConstructorHelpers::FObjectFinder<UMaterialInterface> SkyMaterialFinder(GetDefaultSkyMaterialPath());
		if (SkyMaterialFinder.Succeeded())
		{
			SkySettings.SkyMaterial = SkyMaterialFinder.Object;
			DomeMesh->SetMaterial(0, SkyMaterialFinder.Object);
		}
	}
}

void AAuroraSkyDome::UpdateDomeScale()
{
	if (!DomeMesh)
	{
		return;
	}

	// DomeRadiusMeters 是半径；Sphere 基础半径 50 uu = 0.5 m
	const float RadiusUU = FMath::Max(DomeRadiusMeters, 1.0f) * 100.0f;
	const float Scale = RadiusUU / GSphereBaseRadiusUU;
	DomeMesh->SetWorldScale3D(FVector(Scale));
}

void AAuroraSkyDome::ApplySkyMaterial()
{
	if (!DomeMesh)
	{
		return;
	}

	UMaterialInterface* BaseMaterial = SkySettings.SkyMaterial;
	if (!BaseMaterial)
	{
		BaseMaterial = DomeMesh->GetMaterial(0);
	}

	// 若上一轮已经换成了 MID，要回到它的父材质，因为 MID 不能作为 MID 的父级
	if (UMaterialInstanceDynamic* ExistingMID = Cast<UMaterialInstanceDynamic>(BaseMaterial))
	{
		BaseMaterial = ExistingMID->Parent;
	}
	else if (UMaterialInstance* ExistingMI = Cast<UMaterialInstance>(BaseMaterial))
	{
		BaseMaterial = ExistingMI->Parent;
	}

	if (!BaseMaterial)
	{
		return;
	}

	if (!SkyMID || SkyMID->Parent != BaseMaterial)
	{
		SkyMID = UMaterialInstanceDynamic::Create(BaseMaterial, this, TEXT("AuroraSkyMID"));
	}

	if (SkyMID)
	{
		DomeMesh->SetMaterial(0, SkyMID);
	}
}

UMaterialInstanceDynamic* AAuroraSkyDome::GetSkyMaterialInstance()
{
	if (!SkyMID)
	{
		ApplySkyMaterial();
	}
	return SkyMID;
}

void AAuroraSkyDome::BindSkyCubeTexture(UTexture* InCubeTexture)
{
	UMaterialInstanceDynamic* MID = GetSkyMaterialInstance();
	if (!MID)
	{
		return;
	}

	if (InCubeTexture)
	{
		MID->SetTextureParameterValue(TEXT("AuroraSkyCube"), InCubeTexture);
	}

	MID->SetScalarParameterValue(TEXT("SkyIntensity"), SkySettings.SkyIntensity);
}

void AAuroraSkyDome::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	UpdateDomeScale();
	ApplySkyMaterial();
}

void AAuroraSkyDome::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFollowCamera)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC || !PC->PlayerCameraManager)
	{
		return;
	}

	SetActorLocation(PC->PlayerCameraManager->GetCameraLocation(), false, nullptr, ETeleportType::TeleportPhysics);
}

#if WITH_EDITOR
void AAuroraSkyDome::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UpdateDomeScale();

	if (UMaterialInstanceDynamic* MID = GetSkyMaterialInstance())
	{
		MID->SetScalarParameterValue(TEXT("SkyIntensity"), SkySettings.SkyIntensity);
	}
}
#endif

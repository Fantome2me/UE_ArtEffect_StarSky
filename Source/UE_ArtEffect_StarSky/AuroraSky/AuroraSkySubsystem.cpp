// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuroraSky/AuroraSkySubsystem.h"

#include "AuroraSky/AuroraSkyDome.h"
#include "AuroraSky/AuroraSkyShaderDispatch.h"
#include "Engine/TextureRenderTargetCube.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RHI.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "UE_ArtEffect_StarSky.h"

namespace
{
	/** Cube 分辨率硬上限，避免把 Details 面板里的值调到显存爆掉 */
	constexpr int32 GSkyCubeResolutionMax = 2048;
	constexpr int32 GSkyCubeResolutionMin = 32;
}

// ---------------------------------------------------------------------------
// USubsystem
// ---------------------------------------------------------------------------

void UAuroraSkySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogUE_ArtEffect_StarSky, Log, TEXT("[AuroraSky] Subsystem ready for world '%s'."), *GetNameSafe(GetWorld()));
}

void UAuroraSkySubsystem::Deinitialize()
{
	ReleaseRenderTarget();
	CachedDome = nullptr;
	LastBoundTexture.Reset();
	LastBoundMID.Reset();

	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// FTickableGameObject
// ---------------------------------------------------------------------------

TStatId UAuroraSkySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAuroraSkySubsystem, STATGROUP_Tickables);
}

void UAuroraSkySubsystem::Tick(float DeltaTime)
{
	const UWorld* World = GetWorld();
	if (!World || World->IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	ResolveDome();

	if (!CachedDome)
	{
		// 编辑器里还没有摆穹顶时，什么都不用算
		return;
	}

	const FAuroraSkySettings Settings = GetActiveSettings();

	SkyTime += DeltaTime * Settings.SkyTimeScale;

	EnsureRenderTarget(Settings.CubeResolution);
	if (!SkyCubeRT)
	{
		return;
	}

	BindCubeToDome(Settings);

	// 节流：按 UpdateFrequency 决定这一帧要不要烘
	TimeUntilNextBake -= DeltaTime;
	if (!bNeedsBake && TimeUntilNextBake > 0.0f)
	{
		return;
	}

	bNeedsBake = false;
	TimeUntilNextBake = 1.0f / FMath::Max(Settings.UpdateFrequency, 0.1f);

	DispatchBake(SkyTime + Settings.SkyTimeOffset);
}

// ---------------------------------------------------------------------------
// 公共接口
// ---------------------------------------------------------------------------

UAuroraSkySubsystem* UAuroraSkySubsystem::Get(const UWorld* World)
{
	return World ? World->GetSubsystem<UAuroraSkySubsystem>() : nullptr;
}

void UAuroraSkySubsystem::SetSkyEnabled(bool bInEnabled)
{
	if (bSkyEnabled == bInEnabled)
	{
		return;
	}

	bSkyEnabled = bInEnabled;

	UE_LOG(LogUE_ArtEffect_StarSky, Log, TEXT("[AuroraSky] Sky %s."), bSkyEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void UAuroraSkySubsystem::BakeNow()
{
	ResolveDome();
	if (!CachedDome)
	{
		return;
	}

	const FAuroraSkySettings Settings = GetActiveSettings();
	EnsureRenderTarget(Settings.CubeResolution);
	if (!SkyCubeRT)
	{
		return;
	}

	BindCubeToDome(Settings);
	DispatchBake(SkyTime + Settings.SkyTimeOffset);

	bNeedsBake = false;
	TimeUntilNextBake = 1.0f / FMath::Max(Settings.UpdateFrequency, 0.1f);
}

// ---------------------------------------------------------------------------
// 内部
// ---------------------------------------------------------------------------

void UAuroraSkySubsystem::ResolveDome()
{
	if (IsValid(CachedDome))
	{
		return;
	}

	CachedDome = nullptr;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AAuroraSkyDome> It(World); It; ++It)
	{
		CachedDome = *It;
		return;
	}

	// 关卡的穹顶被删了也要能跑：游戏世界里自动补一个
	if (World->IsGameWorld())
	{
		const FAuroraSkySettings Defaults;
		if (Defaults.bAutoSpawnDome)
		{
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

			CachedDome = World->SpawnActor<AAuroraSkyDome>(FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);
			UE_LOG(LogUE_ArtEffect_StarSky, Log, TEXT("[AuroraSky] No AAuroraSkyDome found, spawned a default dome."));
		}
	}
}

FAuroraSkySettings UAuroraSkySubsystem::GetActiveSettings() const
{
	if (CachedDome)
	{
		return CachedDome->SkySettings;
	}
	return FAuroraSkySettings();
}

void UAuroraSkySubsystem::ReleaseRenderTarget()
{
	if (SkyCubeRT)
	{
		SkyCubeRT->ReleaseResource();
		SkyCubeRT = nullptr;
	}
	CubeResolution = 0;
	LastBoundTexture.Reset();
	LastBoundMID.Reset();
	bLoggedNotReady = false;
	bNeedsBake = true;
}

void UAuroraSkySubsystem::EnsureRenderTarget(int32 Resolution)
{
	const int32 Clamped = FMath::Clamp(Resolution, GSkyCubeResolutionMin, GSkyCubeResolutionMax);
	if (CubeResolution == Clamped && SkyCubeRT)
	{
		return;
	}

	ReleaseRenderTarget();

	UTextureRenderTargetCube* RT = NewObject<UTextureRenderTargetCube>(this, NAME_None, RF_Transient);
	if (!RT)
	{
		UE_LOG(LogUE_ArtEffect_StarSky, Error, TEXT("[AuroraSky] Failed to create cube render target."));
		return;
	}

	RT->bHDR = true;
	// 必须在 Init() 之前设置：CreateResource() 会据此把 bCanCreateUAV 置 1，
	// RHI 侧才会带上 TexCreate_UAV 并生成 UnorderedAccessViewRHI
	RT->bSupportsUAV = true;
	RT->bAutoGenerateMips = false;
	RT->bForceLinearGamma = true;
	RT->ClearColor = FLinearColor::Black;
	RT->Filter = TF_Bilinear;
	RT->SRGB = 0;
	RT->NeverStream = true;
	RT->LODGroup = TEXTUREGROUP_World;

	RT->Init(Clamped, PF_FloatRGBA);
	// Init() 只是 UpdateResource()（延迟创建）；这里同步等资源建好，才能立刻拿 UAV
	RT->UpdateResourceImmediate(true);

	SkyCubeRT = RT;
	CubeResolution = Clamped;

	const double VramMB = (static_cast<double>(Clamped) * Clamped * 6 * 8) / (1024.0 * 1024.0);
	UE_LOG(LogUE_ArtEffect_StarSky, Log,
		TEXT("[AuroraSky] Created %dx%d PF_FloatRGBA cube render target (UAV), %.1f MB."),
		Clamped, Clamped, VramMB);
}

void UAuroraSkySubsystem::BindCubeToDome(const FAuroraSkySettings& Settings)
{
	if (!CachedDome || !SkyCubeRT)
	{
		return;
	}

	UMaterialInstanceDynamic* MID = CachedDome->GetSkyMaterialInstance();

	// 贴图参数会让 MID 重编 uniform expression，所以只在 RT 或 MID 变化时设一次。
	// 注意 MID 也可能被重建（改材质 / 改设置后重新生成），那时新 MID 上没有
	// AuroraSkyCube 参数，会退化成采默认黑 cube —— 天空直接变黑，所以必须重绑。
	if (LastBoundTexture.Get() != SkyCubeRT || LastBoundMID.Get() != MID)
	{
		CachedDome->BindSkyCubeTexture(SkyCubeRT);
		LastBoundTexture = SkyCubeRT;
		LastBoundMID = MID;
	}

	// 标量参数很便宜，每次对齐一次，调面板即时生效
	if (MID)
	{
		MID->SetScalarParameterValue(TEXT("SkyIntensity"), Settings.SkyIntensity);
	}
}

void UAuroraSkySubsystem::DispatchBake(float InSkyTime)
{
	if (!SkyCubeRT)
	{
		return;
	}

	// GetRenderTargetResource() 只能在渲染线程调用（内部有 assert），
	// 游戏线程要用 GameThread_GetRenderTargetResource()：只允许判空，不能解引用。
	FTextureRenderTargetResource* RTResource = SkyCubeRT->GameThread_GetRenderTargetResource();
	if (!RTResource)
	{
		if (!bLoggedNotReady)
		{
			bLoggedNotReady = true;
			UE_LOG(LogUE_ArtEffect_StarSky, Log, TEXT("[AuroraSky] Cube render target resource is not ready yet."));
		}
		return;
	}

	const int32 Size = SkyCubeRT->SizeX;

	// 资源解引用和 dispatch 都放在渲染线程做
	ENQUEUE_RENDER_COMMAND(AuroraSkyBake)(
		[RTResource, Size, InSkyTime](FRHICommandListImmediate& RHICmdList)
		{
			FTextureRenderTargetCubeResource* CubeResource = RTResource->GetTextureRenderTargetCubeResource();
			if (!CubeResource)
			{
				return;
			}

			FTextureRHIRef CubeTexture = CubeResource->TextureRHI;
			FUnorderedAccessViewRHIRef CubeUAV = CubeResource->GetUnorderedAccessViewRHI();
			if (!CubeTexture.IsValid() || !CubeUAV.IsValid())
			{
				return;
			}

			StarSky_DispatchAuroraSky(
				RHICmdList,
				CubeTexture.GetReference(),
				CubeUAV.GetReference(),
				Size,
				InSkyTime);
		});
}

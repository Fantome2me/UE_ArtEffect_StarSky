// Copyright Epic Games, Inc. All Rights Reserved.

#include "AuroraSky/AuroraSkySubsystem.h"

#include "AuroraSky/AuroraSkyDome.h"
#include "AuroraSky/AuroraSkyShaderDispatch.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/TextureRenderTargetCube.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
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

	/** 6 个 cube 面全可见 */
	constexpr int32 GAllFacesMask = (1 << 6) - 1;

	/** 按引擎 cube 面顺序（+X,-X,+Y,-Y,+Z,-Z）返回方向的主轴面号 */
	int32 DominantCubeFace(const FVector& Dir)
	{
		const double Ax = FMath::Abs(Dir.X);
		const double Ay = FMath::Abs(Dir.Y);
		const double Az = FMath::Abs(Dir.Z);

		if (Ax >= Ay && Ax >= Az)
		{
			return Dir.X > 0.0 ? 0 : 1;
		}
		if (Ay >= Az)
		{
			return Dir.Y > 0.0 ? 2 : 3;
		}
		return Dir.Z > 0.0 ? 4 : 5;
	}
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

	// 天空演化时间与调度时钟相互独立：前者可被 SkyTimeScale 缩放/暂停，
	// 后者始终按真实时间推进，保证调度节奏稳定
	SkyTime += DeltaTime * Settings.SkyTimeScale;
	ScheduleClock += DeltaTime;

	EnsureRenderTarget(Settings.CubeResolution);
	if (!SkyCubeRT)
	{
		return;
	}

	// MID / 贴图参数每帧对齐一次（贴图只在 RT 或 MID 变化时重设）
	BindCubeToDome(Settings);

	// RT 刚建好（或刚重建）：先把 6 个面一次性填满，
	// 否则按面调度要等 6 帧才补全，开头会出现黑天空
	if (bNeedsFullBake)
	{
		bNeedsFullBake = false;
		DispatchFaces(0, 6, SkyTime + Settings.SkyTimeOffset);
		for (int32 Face = 0; Face < 6; ++Face)
		{
			FaceLastUpdateTime[Face] = ScheduleClock;
		}
		return;
	}

	// 每帧最多烘 1 个面 → 单帧 GPU 峰值恒定且很小
	const int32 Face = SelectNextFace(Settings);
	if (Face == INDEX_NONE)
	{
		return;
	}

	DispatchFaces(Face, 1, SkyTime + Settings.SkyTimeOffset);
	FaceLastUpdateTime[Face] = ScheduleClock;
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

	// 一次性把 6 个面全烘一遍（会花掉完整成本，仅调试/落地脚本使用）
	DispatchFaces(0, 6, SkyTime + Settings.SkyTimeOffset);

	for (int32 Face = 0; Face < 6; ++Face)
	{
		FaceLastUpdateTime[Face] = ScheduleClock;
	}
}

int32 UAuroraSkySubsystem::GetVisibleFaceMask() const
{
	return ResolveVisibleFaceMask(GetActiveSettings());
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

int32 UAuroraSkySubsystem::ResolveVisibleFaceMask(const FAuroraSkySettings& Settings) const
{
	if (!Settings.bUseViewVisibility)
	{
		return GAllFacesMask;
	}

	// 编辑器视口没有 PlayerController，会自动退化成「全部可见」
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	const APlayerCameraManager* Cam = PC ? PC->PlayerCameraManager : nullptr;
	if (!Cam)
	{
		return GAllFacesMask;
	}

	const FRotator CamRot = Cam->GetCameraRotation();
	const FRotationMatrix Basis(CamRot);
	const FVector Fwd = Basis.GetUnitAxis(EAxis::X);
	const FVector Right = Basis.GetUnitAxis(EAxis::Y);
	const FVector Up = Basis.GetUnitAxis(EAxis::Z);

	const float HalfFovX = FMath::DegreesToRadians(FMath::Clamp(Cam->GetFOVAngle(), 10.0f, 170.0f) * 0.5f);
	const float TanX = FMath::Tan(HalfFovX);
	// 竖直方向按 16:9 估一下即可 —— 这里只用来决定「哪几个面优先」，不需要精确
	const float TanY = TanX * 9.0f / 16.0f;

	const FVector Corners[4] =
	{
		(Fwd + Right * TanX + Up * TanY).GetSafeNormal(),
		(Fwd - Right * TanX + Up * TanY).GetSafeNormal(),
		(Fwd + Right * TanX - Up * TanY).GetSafeNormal(),
		(Fwd - Right * TanX - Up * TanY).GetSafeNormal()
	};

	int32 Mask = 1 << DominantCubeFace(Fwd);
	for (const FVector& Corner : Corners)
	{
		Mask |= 1 << DominantCubeFace(Corner);
	}
	return Mask;
}

int32 UAuroraSkySubsystem::SelectNextFace(const FAuroraSkySettings& Settings) const
{
	const int32 VisibleMask = ResolveVisibleFaceMask(Settings);

	const float VisibleInterval = 1.0f / FMath::Max(Settings.UpdateFrequency, 0.1f);
	const float OffscreenInterval = 1.0f / FMath::Max(Settings.OffscreenFrequency, 0.1f);

	int32 BestFace = INDEX_NONE;
	float BestOverdue = 0.0f;

	for (int32 Face = 0; Face < 6; ++Face)
	{
		const bool bVisible = (VisibleMask & (1 << Face)) != 0;
		const float Interval = bVisible ? VisibleInterval : OffscreenInterval;
		const float Overdue = (ScheduleClock - FaceLastUpdateTime[Face]) - Interval;

		if (Overdue > BestOverdue)
		{
			BestOverdue = Overdue;
			BestFace = Face;
		}
	}

	return BestFace;
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
	bNeedsFullBake = true;

	for (int32 Face = 0; Face < 6; ++Face)
	{
		FaceLastUpdateTime[Face] = -1000.0f;
	}
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

void UAuroraSkySubsystem::DispatchFaces(int32 FirstFace, int32 NumFaces, float InSkyTime)
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
		[RTResource, Size, FirstFace, NumFaces, InSkyTime](FRHICommandListImmediate& RHICmdList)
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
				FirstFace,
				NumFaces,
				InSkyTime);
		});
}

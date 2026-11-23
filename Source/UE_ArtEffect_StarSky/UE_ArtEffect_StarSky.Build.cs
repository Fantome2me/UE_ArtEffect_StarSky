// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UE_ArtEffect_StarSky : ModuleRules
{
	public UE_ArtEffect_StarSky(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		// 星空极光天空盒：全局计算着色器 FNTAuroraSkyCS + Cube RenderTarget UAV 烘焙
		PrivateDependencyModuleNames.AddRange(new string[] {
			"RenderCore",
			"RHI"
		});

		// 全局着色器必须住在 LoadingPhase = PostConfigInit 的模块里，所以单独拆了 StarSkyShaders
		PublicDependencyModuleNames.Add("StarSkyShaders");

		PublicIncludePaths.AddRange(new string[] {
			"UE_ArtEffect_StarSky",
			"UE_ArtEffect_StarSky/Variant_Platforming",
			"UE_ArtEffect_StarSky/Variant_Platforming/Animation",
			"UE_ArtEffect_StarSky/Variant_Combat",
			"UE_ArtEffect_StarSky/Variant_Combat/AI",
			"UE_ArtEffect_StarSky/Variant_Combat/Animation",
			"UE_ArtEffect_StarSky/Variant_Combat/Gameplay",
			"UE_ArtEffect_StarSky/Variant_Combat/Interfaces",
			"UE_ArtEffect_StarSky/Variant_Combat/UI",
			"UE_ArtEffect_StarSky/Variant_SideScrolling",
			"UE_ArtEffect_StarSky/Variant_SideScrolling/AI",
			"UE_ArtEffect_StarSky/Variant_SideScrolling/Gameplay",
			"UE_ArtEffect_StarSky/Variant_SideScrolling/Interfaces",
			"UE_ArtEffect_StarSky/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/**
 * 星空极光天空盒的着色器模块。
 *
 * 这个模块必须是 LoadingPhase = PostConfigInit：
 * 全局着色器（FGlobalShader）的类型注册发生在 DLL 静态初始化期，
 * 一旦晚于 shader types 初始化就会触发
 *   Assertion failed: !AreShaderTypesInitialized()  [Shader.cpp]
 * 并导致编辑器启动崩溃。
 *
 * 所以它只依赖 Core / RenderCore / RHI，不依赖 Engine。
 */
public class StarSkyShaders : ModuleRules
{
	public StarSkyShaders(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"RenderCore",
			"RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
	}
}

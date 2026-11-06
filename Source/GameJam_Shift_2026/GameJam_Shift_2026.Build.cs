// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameJam_Shift_2026 : ModuleRules
{
	public GameJam_Shift_2026(ReadOnlyTargetRules Target) : base(Target)
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
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"GameJam_Shift_2026",
			"GameJam_Shift_2026/Variant_Horror",
			"GameJam_Shift_2026/Variant_Horror/UI",
			"GameJam_Shift_2026/Variant_Shooter",
			"GameJam_Shift_2026/Variant_Shooter/AI",
			"GameJam_Shift_2026/Variant_Shooter/UI",
			"GameJam_Shift_2026/Variant_Shooter/Weapons",
			"GameJam_Shift_2026/Variant_Shooter/Inventory"
		});
	}
}

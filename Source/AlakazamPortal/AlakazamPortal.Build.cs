using UnrealBuildTool;

public class AlakazamPortal : ModuleRules
{
	public AlakazamPortal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RHI",
				"ImageWrapper",
				"WebSockets",
				"Json",
				"JsonUtilities",
				"Slate",
				"SlateCore",
				"UMG",
				"DeveloperSettings",
				"InputCore"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"LevelEditor",
					"EditorStyle",
					"ToolMenus"
				}
			);
		}
	}
}

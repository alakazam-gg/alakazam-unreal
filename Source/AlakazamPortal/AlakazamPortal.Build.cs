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

		// DesktopPlatform for file dialogs (available in editor and standalone)
		PrivateDependencyModuleNames.Add("DesktopPlatform");

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"LevelEditor",
					"EditorStyle",
					"ToolMenus",
					"PropertyEditor"
				}
			);
		}
	}
}

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
				"WebSockets",  // Use UE's native WebSocket
				"Json",
				"JsonUtilities"
			}
		);
	}
}

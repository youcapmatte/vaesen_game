using UnrealBuildTool;

/**
 * Anim graph nodes for Vaesen's anim nodes (the editor side of FAnimNode_VaesenShoulderAim). UncookedOnly: loaded by
 * the editor and by uncooked standalone runs (-game), which read anim Blueprints with their graphs; never cooked.
 */
public class VaesenGameAnimGraph : ModuleRules
{
	public VaesenGameAnimGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "AnimGraph", "AnimGraphRuntime", "VaesenGame" });
		PrivateDependencyModuleNames.AddRange(new string[] { "BlueprintGraph" });
	}
}

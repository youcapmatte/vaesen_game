using UnrealBuildTool;

/** Editor-only tools for Vaesen (never in game, client or server builds). */
public class VaesenGameEditor : ModuleRules
{
	public VaesenGameEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
		PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "UMG", "UMGEditor", "Slate", "SlateCore", "AssetRegistry", "EnhancedInput", "InputCore", "VaesenGame",
			"AssetTools", "MaterialEditor", "GameplayTags", "AnimationCore", "GeometryCore", "GeometryFramework", "GeometryScriptingCore", "SkeletalMeshDescription", "SkeletalMeshUtilitiesCommon", "MeshConversion", "MeshDescription", "RenderCore", "DynamicMesh",
			"GeometryScriptingEditor", "GameplayAbilities", "PhysicsCore" });
	}
}

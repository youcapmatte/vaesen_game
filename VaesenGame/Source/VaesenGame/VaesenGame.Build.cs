// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VaesenGame : ModuleRules
{
	public VaesenGame(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "AIModule", "UMG", "DeveloperSettings", "HTTP", "SlateCore" });

		PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "Json", "JsonUtilities" });

		// The GameLift Server SDK is only compiled into the dedicated server target; clients and the editor never link it.
		if (Target.Type == TargetType.Server)
		{
			PrivateDependencyModuleNames.Add("GameLiftServerSDK");
			PrivateDefinitions.Add("VAESEN_WITH_GAMELIFT=1");
		}
		else
		{
			PrivateDefinitions.Add("VAESEN_WITH_GAMELIFT=0");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

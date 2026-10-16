// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class VaesenGameServerTarget : TargetRules
{
	public VaesenGameServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.V5;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
		ExtraModuleNames.Add("VaesenGame");

		// The GameLift Server SDK plugin doesn't compile file-by-file (see its README), which adaptive unity does for recently edited files.
		bUseAdaptiveUnityBuild = false;
	}
}

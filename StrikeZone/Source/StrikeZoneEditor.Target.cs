// STRIKE ZONE - cible éditeur
using UnrealBuildTool;
using System.Collections.Generic;

public class StrikeZoneEditorTarget : TargetRules
{
	public StrikeZoneEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StrikeZone");
	}
}

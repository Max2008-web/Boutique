// STRIKE ZONE - serveur dédié (nécessite une version d'Unreal Engine compilée depuis les sources)
using UnrealBuildTool;
using System.Collections.Generic;

public class StrikeZoneServerTarget : TargetRules
{
	public StrikeZoneServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StrikeZone");
	}
}

// STRIKE ZONE - cible jeu (Windows, Android, iOS futur)
using UnrealBuildTool;
using System.Collections.Generic;

public class StrikeZoneTarget : TargetRules
{
	public StrikeZoneTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("StrikeZone");
	}
}

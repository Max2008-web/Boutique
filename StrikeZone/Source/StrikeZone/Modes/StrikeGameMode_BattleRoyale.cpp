// STRIKE ZONE - Battle Royale : grande carte, butin, zone dynamique, jusqu'à 30 combattants
#include "Modes/StrikeGameModes.h"
#include "AI/StrikeAIController.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerState.h"
#include "Weapons/StrikeWeaponTypes.h"
#include "World/StrikeArenaBuilder.h"
#include "World/StrikeInteractables.h"
#include "World/StrikePickup.h"

#include "Engine/World.h"
#include "GameFramework/GameSession.h"

AStrikeGameMode_BattleRoyale::AStrikeGameMode_BattleRoyale()
{
	ModeType = EStrikeModeType::BattleRoyale;
	ModeDisplayName = TEXT("BATTLE ROYALE");
	ArenaLayout = EStrikeArenaLayout::Large;
	bUseTeams = false;
	NumTeams = 1;
	TargetCombatants = 20;
	bAllowRespawn = false;
	MatchDuration = 1800;
	DefaultLoadout = { AStrikeWeapon_Pistol::StaticClass() };

	// Attente, rétrécissement, rayon final (fraction du rayon initial), dégâts/s hors zone.
	const auto AddPhase = [this](int32 Wait, int32 Shrink, float Fraction, float Damage)
	{
		FStrikeZonePhase Phase;
		Phase.WaitSeconds = Wait;
		Phase.ShrinkSeconds = Shrink;
		Phase.RadiusFraction = Fraction;
		Phase.DamagePerSecond = Damage;
		Phases.Add(Phase);
	};
	AddPhase(60, 45, 0.60f, 2.f);
	AddPhase(45, 40, 0.35f, 4.f);
	AddPhase(40, 35, 0.18f, 7.f);
	AddPhase(30, 30, 0.07f, 10.f);
	AddPhase(20, 25, 0.0f, 15.f);
}

void AStrikeGameMode_BattleRoyale::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	TargetCombatants = FMath::Clamp(TargetCombatants, 2, MaxCombatants);
	if (GameSession)
	{
		GameSession->MaxPlayers = MaxCombatants;
	}
}

void AStrikeGameMode_BattleRoyale::ConfigureCharacter(AStrikeCharacter* Character, AController* Controller)
{
	Super::ConfigureCharacter(Character, Controller);
	Character->GetHealthComponent()->bRegenerate = false;
}

TArray<TSubclassOf<AStrikeWeapon>> AStrikeGameMode_BattleRoyale::GetLoadoutFor(AController* Controller) const
{
	// Les bots ne ramassent pas d'armes : ils reçoivent une arme principale aléatoire en plus du pistolet.
	if (Cast<AStrikeAIController>(Controller))
	{
		const TSubclassOf<AStrikeWeapon> Primaries[] = {
			AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_SMG::StaticClass(), AStrikeWeapon_Shotgun::StaticClass()
		};
		return { Primaries[FMath::RandRange(0, 2)], AStrikeWeapon_Pistol::StaticClass() };
	}
	return DefaultLoadout;
}

void AStrikeGameMode_BattleRoyale::SetupModeActors(AStrikeArenaBuilder* InArena)
{
	if (!InArena)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const TSubclassOf<AStrikeWeapon> LootWeapons[] = {
		AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_SMG::StaticClass(), AStrikeWeapon_Shotgun::StaticClass(),
		AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_SMG::StaticClass(), AStrikeWeapon_Sniper::StaticClass()
	};

	for (const FVector& Location : InArena->LootLocations)
	{
		AStrikePickup* Pickup = GetWorld()->SpawnActor<AStrikePickup>(AStrikePickup::StaticClass(), Location, FRotator::ZeroRotator, Params);
		if (!Pickup)
		{
			continue;
		}
		const float Roll = FMath::FRand();
		if (Roll < 0.35f)
		{
			Pickup->SetupWeapon(FMath::FRand() < 0.04f ? TSubclassOf<AStrikeWeapon>(AStrikeWeapon_RocketLauncher::StaticClass())
				: LootWeapons[FMath::RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(LootWeapons)) - 1)]);
		}
		else if (Roll < 0.7f)
		{
			Pickup->SetupAmmo(static_cast<EStrikeAmmoType>(FMath::RandRange(0, 3)), 40);
		}
		else if (Roll < 0.85f)
		{
			Pickup->SetupArmor(50);
		}
		else
		{
			Pickup->SetupHealth(50);
		}
	}

	GetWorld()->SpawnActor<AStrikeStormZone>(AStrikeStormZone::StaticClass(), FTransform::Identity, Params);

	InitialRadius = InArena->GetHalfSize() * 1.45f;
}

void AStrikeGameMode_BattleRoyale::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();

	if (InitialRadius <= 0.f)
	{
		InitialRadius = 20000.f;
	}
	PhaseIndex = 0;
	bShrinking = false;
	StartRadius = TargetRadius = InitialRadius;
	StartCenter = TargetCenter = FVector::ZeroVector;
	PhaseTimer = Phases.Num() > 0 ? Phases[0].WaitSeconds : 60;
	CurrentDamagePerSecond = Phases.Num() > 0 ? Phases[0].DamagePerSecond : 1.f;
	SecondsSinceStart = 0;

	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->ZoneCenter = StartCenter;
		GS->ZoneRadius = StartRadius;
		GS->NextZoneCenter = StartCenter;
		GS->NextZoneRadius = StartRadius;
		GS->ZonePhaseTimer = PhaseTimer;
		GS->MulticastAnnouncement(TEXT("Ramassez des armes - la zone va se refermer"), 4.f);
	}
}

void AStrikeGameMode_BattleRoyale::OnMatchTick()
{
	Super::OnMatchTick();
	++SecondsSinceStart;

	UpdateZone();
	ApplyZoneDamage();
	GuideBotsIntoZone();

	const TArray<AStrikeCharacter*> Alive = GetAliveCharacters();
	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->AliveCount = Alive.Num();
	}

	if (SecondsSinceStart > 5 && Alive.Num() <= 1)
	{
		const AStrikeCharacter* Winner = Alive.Num() == 1 ? Alive[0] : nullptr;
		FinishMatch(Winner ? FString::Printf(TEXT("TOP 1 : %s"), *GetDisplayName(Winner->GetController(), Winner)) : TEXT("Aucun survivant"));
	}
}

void AStrikeGameMode_BattleRoyale::UpdateZone()
{
	AStrikeGameState* GS = GetStrikeGameState();
	if (!GS || PhaseIndex >= Phases.Num())
	{
		return;
	}
	const FStrikeZonePhase& Current = Phases[PhaseIndex];

	if (!bShrinking)
	{
		if (PhaseTimer == Current.WaitSeconds)
		{
			// Choisit la prochaine zone à l'intérieur de la zone actuelle.
			StartRadius = GS->ZoneRadius;
			StartCenter = GS->ZoneCenter;
			TargetRadius = InitialRadius * Current.RadiusFraction;
			const float MaxOffset = FMath::Max(0.f, StartRadius - TargetRadius) * 0.8f;
			const FVector2D Offset = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)).GetSafeNormal() * FMath::FRandRange(0.f, MaxOffset);
			TargetCenter = StartCenter + FVector(Offset.X, Offset.Y, 0.f);
			// La zone ne sort jamais de la carte jouable.
			const float Limit = FMath::Max(0.f, InitialRadius / 1.45f - FMath::Min(TargetRadius, InitialRadius / 1.45f));
			TargetCenter.X = FMath::Clamp(TargetCenter.X, -Limit, Limit);
			TargetCenter.Y = FMath::Clamp(TargetCenter.Y, -Limit, Limit);
			GS->NextZoneCenter = TargetCenter;
			GS->NextZoneRadius = TargetRadius;
		}
		if (--PhaseTimer <= 0)
		{
			bShrinking = true;
			PhaseTimer = Current.ShrinkSeconds;
			CurrentDamagePerSecond = Current.DamagePerSecond;
			GS->MulticastAnnouncement(TEXT("LA ZONE SE REFERME"), 3.f);
		}
	}
	else
	{
		--PhaseTimer;
		const float Alpha = 1.f - static_cast<float>(FMath::Max(PhaseTimer, 0)) / FMath::Max(Current.ShrinkSeconds, 1);
		GS->ZoneRadius = FMath::Lerp(StartRadius, TargetRadius, Alpha);
		GS->ZoneCenter = FMath::Lerp(StartCenter, TargetCenter, Alpha);
		if (PhaseTimer <= 0)
		{
			bShrinking = false;
			++PhaseIndex;
			PhaseTimer = Phases.IsValidIndex(PhaseIndex) ? Phases[PhaseIndex].WaitSeconds : 0;
		}
	}

	GS->bZoneShrinking = bShrinking;
	GS->ZonePhaseTimer = PhaseTimer;
}

void AStrikeGameMode_BattleRoyale::ApplyZoneDamage()
{
	const AStrikeGameState* GS = GetStrikeGameState();
	if (!GS)
	{
		return;
	}
	for (AStrikeCharacter* Character : GetAliveCharacters())
	{
		if (FVector::Dist2D(Character->GetActorLocation(), GS->ZoneCenter) > GS->ZoneRadius)
		{
			Character->GetHealthComponent()->ApplyDamage(CurrentDamagePerSecond, nullptr, nullptr, EStrikeHitZone::Torso, Character->GetActorLocation());
		}
	}
}

void AStrikeGameMode_BattleRoyale::GuideBotsIntoZone()
{
	const AStrikeGameState* GS = GetStrikeGameState();
	if (!GS)
	{
		return;
	}
	const FVector SafeCenter = bShrinking ? FVector(GS->ZoneCenter) : FVector(GS->NextZoneCenter);
	const float SafeRadius = bShrinking ? GS->ZoneRadius : GS->NextZoneRadius;

	for (AStrikeAIController* Bot : Bots)
	{
		const APawn* Pawn = Bot ? Bot->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		if (FVector::Dist2D(Pawn->GetActorLocation(), SafeCenter) > SafeRadius * 0.8f)
		{
			const FVector2D Offset = FVector2D(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * SafeRadius * 0.4f;
			Bot->SetObjective(SafeCenter + FVector(Offset.X, Offset.Y, 100.f), false);
		}
		else
		{
			Bot->ClearObjective();
		}
	}
}

void AStrikeGameMode_BattleRoyale::OnMatchTimeExpired()
{
	FinishMatch(TEXT("Temps écoulé - pas de vainqueur"));
}

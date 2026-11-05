// STRIKE ZONE - Tactique 5v5 : manches, aucune réapparition pendant la manche, charge à poser / désamorcer
#include "Modes/StrikeGameModes.h"
#include "AI/StrikeAIController.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerState.h"
#include "Weapons/StrikeWeaponTypes.h"
#include "World/StrikeArenaBuilder.h"
#include "World/StrikeInteractables.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

AStrikeGameMode_Tactical::AStrikeGameMode_Tactical()
{
	ModeType = EStrikeModeType::Tactical;
	ModeDisplayName = TEXT("TACTIQUE - 5 contre 5");
	ArenaLayout = EStrikeArenaLayout::Arena;
	bUseTeams = true;
	NumTeams = 2;
	TargetCombatants = 10;
	bAllowRespawn = false;
	MatchDuration = 0; // le chronomètre de manche est géré ici
	DefaultLoadout = { AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_Pistol::StaticClass() };
}

uint8 AStrikeGameMode_Tactical::GetAttackingTeam() const
{
	// Changement de camp à la mi-temps.
	return RoundNumber <= RoundsToWin - 1 ? 0 : 1;
}

void AStrikeGameMode_Tactical::ConfigureCharacter(AStrikeCharacter* Character, AController* Controller)
{
	Super::ConfigureCharacter(Character, Controller);
	UHealthComponent* Health = Character->GetHealthComponent();
	Health->bRegenerate = false;
	Health->StartingArmor = 50.f;
	Health->AddArmor(50.f);
}

void AStrikeGameMode_Tactical::SetupModeActors(AStrikeArenaBuilder* InArena)
{
	if (!InArena)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const TCHAR* Labels[] = { TEXT("A"), TEXT("B"), TEXT("C") };
	for (int32 i = 0; i < InArena->ObjectiveLocations.Num(); ++i)
	{
		if (AStrikeObjectiveZone* Site = GetWorld()->SpawnActor<AStrikeObjectiveZone>(AStrikeObjectiveZone::StaticClass(), InArena->ObjectiveLocations[i], FRotator::ZeroRotator, Params))
		{
			Site->ObjectiveType = EStrikeObjectiveType::ChargeSite;
			Site->Label = Labels[i % static_cast<int32>(UE_ARRAY_COUNT(Labels))];
			Site->RefreshVisual();
			Sites.Add(Site);
		}
	}
}

void AStrikeGameMode_Tactical::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	GetWorldTimerManager().SetTimer(ObjectiveTimer, this, &AStrikeGameMode_Tactical::ObjectiveTick, 0.1f, true);
	StartRound();
}

void AStrikeGameMode_Tactical::SetFrozen(bool bFrozen)
{
	for (AStrikeCharacter* Character : GetAliveCharacters())
	{
		UCharacterMovementComponent* Move = Character->GetCharacterMovement();
		if (bFrozen)
		{
			Character->StopFire();
			Move->DisableMovement();
		}
		else
		{
			Move->SetMovementMode(MOVE_Walking);
		}
	}
}

void AStrikeGameMode_Tactical::StartRound()
{
	++RoundNumber;
	Phase = EStrikeRoundPhase::PreRound;
	PhaseTimer = PreRoundSeconds;
	PlantProgress = 0.f;
	DefuseProgress = 0.f;
	PlantedSite = nullptr;

	for (AStrikeObjectiveZone* Site : Sites)
	{
		Site->bChargePlanted = false;
		Site->SetZoneActive(true);
	}

	// Tout le monde réapparaît en début de manche (humains et bots).
	for (AController* Controller : GetCombatants())
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			Pawn->Destroy();
		}
		if (AStrikePlayerState* PS = Controller->GetPlayerState<AStrikePlayerState>())
		{
			PS->SetEliminated(false);
		}
		RestartPlayer(Controller);
	}
	SetFrozen(true);
	UpdateBotObjectives();

	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->RoundNumber = RoundNumber;
		GS->AttackingTeam = GetAttackingTeam();
		GS->ObjectiveState = 0;
		GS->ObjectiveProgress = 0.f;
		GS->RemainingTime = RoundSeconds;
		GS->StatusText = TEXT("Préparation...");
		GS->MulticastAnnouncement(FString::Printf(TEXT("MANCHE %d - %s ATTAQUENT"), RoundNumber, GetAttackingTeam() == 0 ? TEXT("BLEUS") : TEXT("ROUGES")), 3.f);
	}
}

void AStrikeGameMode_Tactical::EndRound(uint8 WinningTeam, const FString& Reason)
{
	if (Phase == EStrikeRoundPhase::PostRound)
	{
		return;
	}
	Phase = EStrikeRoundPhase::PostRound;
	PhaseTimer = PostRoundSeconds;

	AStrikeGameState* GS = GetStrikeGameState();
	if (!GS)
	{
		return;
	}
	if (GS->TeamScores.IsValidIndex(WinningTeam))
	{
		++GS->TeamScores[WinningTeam];
	}
	GS->ObjectiveState = 0;
	GS->StatusText.Reset();
	GS->MulticastAnnouncement(FString::Printf(TEXT("%s REMPORTENT LA MANCHE - %s"), WinningTeam == 0 ? TEXT("BLEUS") : TEXT("ROUGES"), *Reason), 4.f);

	for (AStrikeAIController* Bot : Bots)
	{
		if (Bot)
		{
			Bot->ClearObjective();
		}
	}
}

void AStrikeGameMode_Tactical::OnMatchTick()
{
	Super::OnMatchTick();
	AStrikeGameState* GS = GetStrikeGameState();
	if (!GS)
	{
		return;
	}

	switch (Phase)
	{
	case EStrikeRoundPhase::PreRound:
		if (--PhaseTimer <= 0)
		{
			Phase = EStrikeRoundPhase::Live;
			SetFrozen(false);
			GS->StatusText.Reset();
		}
		break;

	case EStrikeRoundPhase::Live:
	{
		const uint8 Attackers = GetAttackingTeam();
		const uint8 Defenders = GetDefendingTeam();
		const int32 AliveAttackers = GetAliveCharacters(Attackers).Num();
		const int32 AliveDefenders = GetAliveCharacters(Defenders).Num();

		if (PlantedSite)
		{
			GS->RemainingTime = FMath::Max(0, GS->RemainingTime - 1);
			if (GS->RemainingTime <= 0)
			{
				EndRound(Attackers, TEXT("la charge a explosé"));
			}
			else if (AliveDefenders == 0)
			{
				EndRound(Attackers, TEXT("défenseurs éliminés"));
			}
		}
		else
		{
			GS->RemainingTime = FMath::Max(0, GS->RemainingTime - 1);
			if (AliveAttackers == 0)
			{
				EndRound(Defenders, TEXT("attaquants éliminés"));
			}
			else if (AliveDefenders == 0)
			{
				EndRound(Attackers, TEXT("défenseurs éliminés"));
			}
			else if (GS->RemainingTime <= 0)
			{
				EndRound(Defenders, TEXT("temps écoulé"));
			}
		}
		break;
	}

	case EStrikeRoundPhase::PostRound:
		if (--PhaseTimer <= 0)
		{
			if (GS->GetTeamScore(0) >= RoundsToWin || GS->GetTeamScore(1) >= RoundsToWin)
			{
				FinishMatch(GS->GetTeamScore(0) >= RoundsToWin ? TEXT("Victoire de l'équipe BLEUE") : TEXT("Victoire de l'équipe ROUGE"));
				return;
			}
			StartRound();
		}
		break;
	}
}

void AStrikeGameMode_Tactical::ObjectiveTick()
{
	if (Phase != EStrikeRoundPhase::Live || IsMatchFinished())
	{
		return;
	}
	AStrikeGameState* GS = GetStrikeGameState();
	if (!GS)
	{
		return;
	}
	const float Step = 0.1f;
	const uint8 Attackers = GetAttackingTeam();
	const uint8 Defenders = GetDefendingTeam();

	if (!PlantedSite)
	{
		AStrikeObjectiveZone* PlantingSite = nullptr;
		for (AStrikeObjectiveZone* Site : Sites)
		{
			for (const AStrikeCharacter* Character : Site->GetCharactersInside())
			{
				if (Character->GetTeamId() == Attackers && Character->IsInteractHeld())
				{
					PlantingSite = Site;
				}
			}
		}

		if (PlantingSite)
		{
			PlantProgress += Step / FMath::Max(PlantSeconds, 0.1f);
			GS->ObjectiveState = 1;
			GS->ObjectiveProgress = PlantProgress;
			if (PlantProgress >= 1.f)
			{
				PlantedSite = PlantingSite;
				PlantedSite->bChargePlanted = true;
				PlantedSite->RefreshVisual();
				for (AStrikeObjectiveZone* Site : Sites)
				{
					if (Site != PlantedSite)
					{
						Site->SetZoneActive(false);
					}
				}
				GS->ObjectiveState = 2;
				GS->RemainingTime = DetonationSeconds;
				GS->MulticastAnnouncement(FString::Printf(TEXT("CHARGE POSÉE SUR LE SITE %s"), *PlantedSite->Label), 3.f);
				UpdateBotObjectives();
			}
		}
		else
		{
			PlantProgress = FMath::Max(0.f, PlantProgress - Step);
			GS->ObjectiveState = PlantProgress > 0.f ? 1 : 0;
			GS->ObjectiveProgress = PlantProgress;
		}
	}
	else
	{
		bool bDefusing = false;
		for (const AStrikeCharacter* Character : PlantedSite->GetCharactersInside())
		{
			bDefusing |= Character->GetTeamId() == Defenders && Character->IsInteractHeld();
		}
		if (bDefusing)
		{
			DefuseProgress += Step / FMath::Max(DefuseSeconds, 0.1f);
			GS->ObjectiveState = 3;
			GS->ObjectiveProgress = DefuseProgress;
			if (DefuseProgress >= 1.f)
			{
				EndRound(Defenders, TEXT("charge désamorcée"));
			}
		}
		else
		{
			DefuseProgress = 0.f;
			GS->ObjectiveState = 2;
			GS->ObjectiveProgress = 0.f;
		}
	}
}

void AStrikeGameMode_Tactical::UpdateBotObjectives()
{
	if (Sites.Num() == 0)
	{
		return;
	}
	const uint8 Attackers = GetAttackingTeam();
	// Les attaquants poussent ensemble vers un site choisi par manche ; les défenseurs se répartissent.
	const int32 AttackSiteIndex = RoundNumber % Sites.Num();
	int32 DefenderIndex = 0;

	for (AStrikeAIController* Bot : Bots)
	{
		if (!Bot)
		{
			continue;
		}
		Bot->bRoamTowardEnemies = false;
		const bool bAttacker = Bot->GetTeamId() == Attackers;
		if (PlantedSite)
		{
			// Charge posée : tout le monde converge vers le site ; les défenseurs désamorcent.
			Bot->SetObjective(PlantedSite->GetActorLocation(), !bAttacker);
		}
		else if (bAttacker)
		{
			Bot->SetObjective(Sites[AttackSiteIndex]->GetActorLocation(), true);
		}
		else
		{
			Bot->SetObjective(Sites[DefenderIndex++ % Sites.Num()]->GetActorLocation(), false);
		}
	}
}

// STRIKE ZONE - Entraînement : stand de tir, cibles mobiles, bots passifs, statistiques de précision
#include "Modes/StrikeGameModes.h"
#include "AI/StrikeAIController.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/WeaponComponent.h"
#include "Game/StrikeGameState.h"
#include "Weapons/StrikeWeaponTypes.h"
#include "World/StrikeArenaBuilder.h"
#include "World/StrikeInteractables.h"
#include "World/StrikePickup.h"

#include "Engine/World.h"
#include "TimerManager.h"

AStrikeGameMode_Training::AStrikeGameMode_Training()
{
	ModeType = EStrikeModeType::Training;
	ModeDisplayName = TEXT("ENTRAÎNEMENT - Stand de tir");
	ArenaLayout = EStrikeArenaLayout::Range;
	bUseTeams = true;
	NumTeams = 2;
	TargetCombatants = 0;
	bAllowRespawn = true;
	RespawnDelay = 1.f;
	MatchDuration = 0;
	DefaultLoadout = { AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_Pistol::StaticClass() };
}

void AStrikeGameMode_Training::ConfigureCharacter(AStrikeCharacter* Character, AController* Controller)
{
	Super::ConfigureCharacter(Character, Controller);
	if (Controller && Controller->IsPlayerController())
	{
		Character->GetInventoryComponent()->bInfiniteAmmo = true;
		Character->GetHealthComponent()->RegenDelay = 2.f;
		Character->GetHealthComponent()->RegenPerSecond = 50.f;
	}
}

void AStrikeGameMode_Training::SetupModeActors(AStrikeArenaBuilder* InArena)
{
	if (!InArena)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FRotator FacingPlayer(0.f, 180.f, 0.f);

	for (const FVector& Location : InArena->TargetLocations)
	{
		GetWorld()->SpawnActor<AStrikeTarget>(AStrikeTarget::StaticClass(), Location, FacingPlayer, Params);
	}
	for (int32 i = 0; i < InArena->MovingTargetLocations.Num(); ++i)
	{
		if (AStrikeTarget* Target = GetWorld()->SpawnActor<AStrikeTarget>(AStrikeTarget::StaticClass(), InArena->MovingTargetLocations[i], FacingPlayer, Params))
		{
			Target->MoveAmplitude = 450.f;
			Target->MoveSpeed = i % 2 == 0 ? 1.3f : 0.8f;
		}
	}

	const TSubclassOf<AStrikeWeapon> Rack[] = {
		AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_SMG::StaticClass(), AStrikeWeapon_Shotgun::StaticClass(),
		AStrikeWeapon_Sniper::StaticClass(), AStrikeWeapon_RocketLauncher::StaticClass(), AStrikeWeapon_Pistol::StaticClass()
	};
	for (int32 i = 0; i < InArena->RackLocations.Num(); ++i)
	{
		if (AStrikePickup* Pickup = GetWorld()->SpawnActor<AStrikePickup>(AStrikePickup::StaticClass(), InArena->RackLocations[i], FRotator::ZeroRotator, Params))
		{
			Pickup->SetupWeapon(Rack[i % static_cast<int32>(UE_ARRAY_COUNT(Rack))]);
			Pickup->RespawnTime = 1.f;
		}
	}

	for (const FVector& Location : InArena->BotLocations)
	{
		SpawnTrainingBot(Location);
	}
}

void AStrikeGameMode_Training::SpawnTrainingBot(const FVector& Location)
{
	const FTransform SpawnTransform(FRotator(0.f, 180.f, 0.f), Location + FVector(0.f, 0.f, 50.f));
	AStrikeCharacter* Bot = GetWorld()->SpawnActorDeferred<AStrikeCharacter>(AStrikeCharacter::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Bot)
	{
		return;
	}
	Bot->AutoPossessAI = EAutoPossessAI::Disabled;
	Bot->FinishSpawning(SpawnTransform);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStrikeEnemyAIController* AI = GetWorld()->SpawnActor<AStrikeEnemyAIController>(AStrikeEnemyAIController::StaticClass(), Params);
	if (!AI)
	{
		Bot->Destroy();
		return;
	}
	AI->bPassive = true;
	AI->SetTeamId(1);
	AI->SetDisplayLabel(TEXT("Bot d'entraînement"));
	AI->SetDifficulty(EStrikeAIDifficulty::Easy);
	AI->SetHomeLocation(Location, 700.f);
	AI->Possess(Bot);
	Bot->GetHealthComponent()->bRegenerate = true;
	Bot->GetWeaponComponent()->GiveWeapon(AStrikeWeapon_AssaultRifle::StaticClass(), true);
}

void AStrikeGameMode_Training::OnKill(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot)
{
	const AStrikeCharacter* Victim = Cast<AStrikeCharacter>(VictimPawn);
	if (!Victim || Victim->GetTeamId() != 1)
	{
		return;
	}
	// Le bot réapparaît à son point de départ.
	const FVector Home = Victim->GetActorLocation();
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, Home]()
	{
		const AStrikeArenaBuilder* CurrentArena = GetArena();
		FVector Location = Home;
		if (CurrentArena && CurrentArena->BotLocations.Num() > 0)
		{
			Location = CurrentArena->BotLocations[FMath::RandRange(0, CurrentArena->BotLocations.Num() - 1)];
		}
		SpawnTrainingBot(Location);
	}), 3.f, false);
}

// STRIKE ZONE - mode Zombies : vagues, types, boss, achats muraux, monnaie de partie, coopération
#include "Modes/StrikeGameModes.h"
#include "AI/StrikeZombie.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerController.h"
#include "Game/StrikePlayerState.h"
#include "Weapons/StrikeWeaponTypes.h"
#include "World/StrikeArenaBuilder.h"
#include "World/StrikeInteractables.h"
#include "World/StrikePickup.h"
#include "World/StrikeSpawnPoint.h"

#include "Engine/World.h"
#include "EngineUtils.h"

AStrikeGameMode_Zombies::AStrikeGameMode_Zombies()
{
	ModeType = EStrikeModeType::Zombies;
	ModeDisplayName = TEXT("ZOMBIES - Survie coopérative");
	ArenaLayout = EStrikeArenaLayout::Arena;
	bUseTeams = true;
	NumTeams = 1;
	TargetCombatants = 0;
	bAllowRespawn = false;
	MatchDuration = 0;
	DefaultLoadout = { AStrikeWeapon_Pistol::StaticClass() };
}

void AStrikeGameMode_Zombies::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	if (AStrikePlayerState* PS = NewPlayer->GetPlayerState<AStrikePlayerState>())
	{
		if (PS->GetPoints() == 0)
		{
			PS->AddPoints(StartingPoints);
		}
	}
}

void AStrikeGameMode_Zombies::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	bInWave = false;
	IntermissionLeft = 5;
}

void AStrikeGameMode_Zombies::SetupModeActors(AStrikeArenaBuilder* InArena)
{
	if (!InArena)
	{
		return;
	}
	struct FBuyEntry { TSubclassOf<AStrikeWeapon> Class; int32 Price; };
	const FBuyEntry Entries[] = {
		{ AStrikeWeapon_SMG::StaticClass(), 750 },
		{ AStrikeWeapon_Shotgun::StaticClass(), 1000 },
		{ AStrikeWeapon_AssaultRifle::StaticClass(), 1200 },
		{ AStrikeWeapon_Sniper::StaticClass(), 1500 },
		{ AStrikeWeapon_RocketLauncher::StaticClass(), 3000 },
	};

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 i = 0; i < InArena->BuyLocations.Num(); ++i)
	{
		const FTransform& T = InArena->BuyLocations[i];
		if (AStrikeWeaponBuy* Buy = GetWorld()->SpawnActor<AStrikeWeaponBuy>(AStrikeWeaponBuy::StaticClass(), T, Params))
		{
			const FBuyEntry& Entry = Entries[i % static_cast<int32>(UE_ARRAY_COUNT(Entries))];
			Buy->Setup(Entry.Class, Entry.Price);
		}
	}

	// Caisses de munitions et d'armure qui réapparaissent au centre.
	const FVector Crates[] = { FVector(0.f, 450.f, 60.f), FVector(0.f, -450.f, 60.f), FVector(450.f, 0.f, 210.f) };
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(Crates)); ++i)
	{
		if (AStrikePickup* Pickup = GetWorld()->SpawnActor<AStrikePickup>(AStrikePickup::StaticClass(), Crates[i], FRotator::ZeroRotator, Params))
		{
			Pickup->RespawnTime = 45.f;
			if (i == 2)
			{
				Pickup->SetupArmor(50);
			}
			else
			{
				Pickup->SetupAmmo(i == 0 ? EStrikeAmmoType::Pistol : EStrikeAmmoType::Rifle, 60);
			}
		}
	}
}

int32 AStrikeGameMode_Zombies::CountAliveZombies() const
{
	int32 Count = 0;
	for (TActorIterator<AStrikeZombie> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead())
		{
			++Count;
		}
	}
	return Count;
}

int32 AStrikeGameMode_Zombies::CountAlivePlayers() const
{
	return GetAliveCharacters(0).Num();
}

void AStrikeGameMode_Zombies::OnMatchTick()
{
	Super::OnMatchTick();
	AStrikeGameState* GS = GetStrikeGameState();
	if (!GS)
	{
		return;
	}

	if (!bInWave)
	{
		GS->StatusText = FString::Printf(TEXT("Prochaine vague dans %d s - achetez des armes aux murs !"), IntermissionLeft);
		if (--IntermissionLeft <= 0)
		{
			StartWave();
		}
		return;
	}

	const int32 Alive = CountAliveZombies();

	// Apparition progressive, plafonnée pour les performances (mobile).
	int32 SpawnBudget = 3;
	if (bBossPending)
	{
		SpawnZombie(EStrikeZombieType::Boss);
		bBossPending = false;
		--SpawnBudget;
	}
	while (SpawnBudget-- > 0 && ZombiesToSpawn > 0 && Alive + (3 - SpawnBudget) <= MaxSimultaneousZombies)
	{
		const float Roll = FMath::FRand();
		EStrikeZombieType Type = EStrikeZombieType::Walker;
		if (CurrentWave >= 3 && Roll < 0.12f)
		{
			Type = EStrikeZombieType::Brute;
		}
		else if (CurrentWave >= 2 && Roll < 0.45f)
		{
			Type = EStrikeZombieType::Runner;
		}
		if (SpawnZombie(Type))
		{
			--ZombiesToSpawn;
		}
		else
		{
			break;
		}
	}

	const int32 AliveNow = CountAliveZombies();
	GS->EnemiesRemaining = AliveNow + ZombiesToSpawn;
	GS->StatusText.Reset();

	if (ZombiesToSpawn <= 0 && AliveNow == 0 && !bBossPending)
	{
		EndWave();
		return;
	}

	if (GetNumPlayers() > 0 && CountAlivePlayers() == 0)
	{
		const int32 Survived = FMath::Max(0, CurrentWave - 1);
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (AStrikePlayerController* PC = Cast<AStrikePlayerController>(It->Get()))
			{
				PC->ClientRecordZombieWave(Survived);
			}
		}
		FinishMatch(FString::Printf(TEXT("Submergés à la vague %d"), CurrentWave));
	}
}

void AStrikeGameMode_Zombies::StartWave()
{
	++CurrentWave;
	bInWave = true;
	const int32 PlayerFactor = FMath::Max(1, GetNumPlayers());
	ZombiesToSpawn = FMath::RoundToInt((BaseZombiesPerWave + ZombiesPerWaveGrowth * (CurrentWave - 1)) * (1.f + 0.5f * (PlayerFactor - 1)));
	bBossPending = BossWaveInterval > 0 && CurrentWave % BossWaveInterval == 0;

	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->CurrentWave = CurrentWave;
		GS->EnemiesRemaining = ZombiesToSpawn + (bBossPending ? 1 : 0);
		GS->MulticastAnnouncement(bBossPending ? FString::Printf(TEXT("VAGUE %d - LE BOSS ARRIVE"), CurrentWave) : FString::Printf(TEXT("VAGUE %d"), CurrentWave), 3.f);
	}
}

void AStrikeGameMode_Zombies::EndWave()
{
	bInWave = false;
	IntermissionLeft = IntermissionSeconds;

	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->MulticastAnnouncement(FString::Printf(TEXT("Vague %d survécue !"), CurrentWave), 3.f);
	}

	// Les joueurs tombés reviennent ; les survivants sont soignés et reçoivent un bonus.
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}
		if (AStrikePlayerState* PS = PC->GetPlayerState<AStrikePlayerState>())
		{
			PS->AddPoints(100 + CurrentWave * 20);
		}
		AStrikeCharacter* Character = Cast<AStrikeCharacter>(PC->GetPawn());
		if (!Character || Character->IsDead())
		{
			RestartPlayer(PC);
		}
		else
		{
			Character->GetHealthComponent()->Heal(1000.f);
		}
	}
}

AStrikeZombie* AStrikeGameMode_Zombies::SpawnZombie(EStrikeZombieType Type)
{
	TArray<AStrikeSpawnPoint*> Points;
	for (TActorIterator<AStrikeSpawnPoint> It(GetWorld()); It; ++It)
	{
		if (It->SpawnType == EStrikeSpawnType::Zombie)
		{
			Points.Add(*It);
		}
	}
	if (Points.Num() == 0)
	{
		return nullptr;
	}

	// Préfère les points éloignés des joueurs (pas d'apparition dans le dos à bout portant).
	const TArray<AStrikeCharacter*> Players = GetAliveCharacters(0);
	AStrikeSpawnPoint* Chosen = nullptr;
	for (int32 Attempt = 0; Attempt < 6 && !Chosen; ++Attempt)
	{
		AStrikeSpawnPoint* Candidate = Points[FMath::RandRange(0, Points.Num() - 1)];
		bool bTooClose = false;
		for (const AStrikeCharacter* Player : Players)
		{
			bTooClose |= FVector::Dist(Player->GetActorLocation(), Candidate->GetActorLocation()) < 900.f;
		}
		if (!bTooClose || Attempt == 5)
		{
			Chosen = Candidate;
		}
	}

	const FTransform SpawnTransform(Chosen->GetActorRotation(), Chosen->GetActorLocation() + FVector(FMath::FRandRange(-100.f, 100.f), FMath::FRandRange(-100.f, 100.f), Type == EStrikeZombieType::Boss ? 80.f : 0.f));
	AStrikeZombie* Zombie = GetWorld()->SpawnActorDeferred<AStrikeZombie>(AStrikeZombie::StaticClass(), SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Zombie)
	{
		return nullptr;
	}
	Zombie->InitZombie(Type, 1.f + HealthGrowthPerWave * (CurrentWave - 1));
	Zombie->FinishSpawning(SpawnTransform);
	return Zombie;
}

void AStrikeGameMode_Zombies::NotifyDamageDealt(AController* InstigatorController, AActor* Victim, float Damage, EStrikeHitZone Zone)
{
	if (Damage <= 0.f || !Cast<AStrikeZombie>(Victim))
	{
		return;
	}
	if (AStrikePlayerState* PS = InstigatorController ? InstigatorController->GetPlayerState<AStrikePlayerState>() : nullptr)
	{
		PS->AddPoints(PointsPerHit);
	}
}

void AStrikeGameMode_Zombies::OnKill(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot)
{
	if (!Cast<AStrikeZombie>(VictimPawn))
	{
		return;
	}
	if (AStrikePlayerState* PS = Killer ? Killer->GetPlayerState<AStrikePlayerState>() : nullptr)
	{
		PS->AddPoints(bHeadshot ? PointsPerHeadshotKill : PointsPerKill);
	}
}

void AStrikeGameMode_Zombies::OnPlayerEliminated(AController* VictimController)
{
	// Pas d'élimination définitive : le joueur reviendra à la fin de la vague. En attendant, il observe.
	if (APlayerController* PC = Cast<APlayerController>(VictimController))
	{
		SpectateSomeone(PC);
	}
}

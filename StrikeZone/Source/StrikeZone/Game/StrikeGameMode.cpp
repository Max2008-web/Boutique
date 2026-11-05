// STRIKE ZONE - règles de partie communes (base de tous les modes)
#include "Game/StrikeGameMode.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerState.h"
#include "Game/StrikePlayerController.h"
#include "UI/StrikeHUD.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/WeaponComponent.h"
#include "Weapons/StrikeWeapon.h"
#include "Weapons/StrikeWeaponTypes.h"
#include "Weapons/StrikeProjectile.h"
#include "AI/StrikeAIController.h"
#include "World/StrikeArenaBuilder.h"
#include "World/StrikeSpawnPoint.h"
#include "StrikeZone.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AStrikeGameMode::AStrikeGameMode()
{
	DefaultPawnClass = AStrikeCharacter::StaticClass();
	PlayerControllerClass = AStrikePlayerController::StaticClass();
	PlayerStateClass = AStrikePlayerState::StaticClass();
	GameStateClass = AStrikeGameState::StaticClass();
	HUDClass = AStrikeHUD::StaticClass();

	BotControllerClass = AStrikeAIController::StaticClass();
	BotPawnClass = AStrikeCharacter::StaticClass();

	DefaultLoadout = { AStrikeWeapon_AssaultRifle::StaticClass(), AStrikeWeapon_Pistol::StaticClass() };

	bDelayedStart = false;
	bStartPlayersAsSpectators = false;
	InactivePlayerStateLifeSpan = 120.f; // reconnexion : les statistiques sont conservées 2 minutes
}

AStrikeGameState* AStrikeGameMode::GetStrikeGameState() const
{
	return GetGameState<AStrikeGameState>();
}

void AStrikeGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Options d'URL : ?Bots=11?Difficulty=2?Time=300?Score=50
	TargetCombatants = UGameplayStatics::GetIntOption(Options, TEXT("Bots"), TargetCombatants);
	BotDifficulty = static_cast<EStrikeAIDifficulty>(FMath::Clamp(UGameplayStatics::GetIntOption(Options, TEXT("Difficulty"), static_cast<int32>(BotDifficulty)), 0, 3));
	MatchDuration = UGameplayStatics::GetIntOption(Options, TEXT("Time"), MatchDuration);
	ScoreLimit = UGameplayStatics::GetIntOption(Options, TEXT("Score"), ScoreLimit);
}

void AStrikeGameMode::InitGameState()
{
	Super::InitGameState();
	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->ModeType = ModeType;
		GS->ModeName = ModeDisplayName;
		GS->bUseTeams = bUseTeams;
		GS->ScoreLimit = ScoreLimit;
		GS->RemainingTime = MatchDuration;
		GS->TeamScores.Init(0, FMath::Max(NumTeams, 1));
	}
}

void AStrikeGameMode::StartPlay()
{
	// L'arène doit exister avant l'apparition des joueurs (déclenchée par Super::StartPlay).
	EnsureArena();
	Super::StartPlay();
	GetWorldTimerManager().SetTimer(MatchTimerHandle, this, &AStrikeGameMode::MatchTimerTick, 1.f, true);
}

void AStrikeGameMode::EnsureArena()
{
	UWorld* World = GetWorld();
	for (TActorIterator<AStrikeArenaBuilder> It(World); It; ++It)
	{
		Arena = *It;
		Arena->BuildArena();
		return;
	}

	bool bHasSpawnPoints = false;
	for (TActorIterator<AStrikeSpawnPoint> It(World); It; ++It)
	{
		bHasSpawnPoints = true;
		break;
	}

	if (!bAutoBuildArena || bHasSpawnPoints)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	Params.bDeferConstruction = true;
	Arena = World->SpawnActor<AStrikeArenaBuilder>(AStrikeArenaBuilder::StaticClass(), FTransform::Identity, Params);
	if (Arena)
	{
		Arena->Layout = ArenaLayout;
		Arena->Seed = FMath::Rand();
		Arena->FinishSpawning(FTransform::Identity);
		Arena->BuildArena();
		UE_LOG(LogStrike, Log, TEXT("Arène procédurale construite (layout %d)"), static_cast<int32>(ArenaLayout));
	}
}

void AStrikeGameMode::HandleMatchHasStarted()
{
	SetupModeActors(Arena);
	Super::HandleMatchHasStarted();
	BalanceBots();
}

void AStrikeGameMode::HandleMatchHasEnded()
{
	Super::HandleMatchHasEnded();

	for (AStrikeCharacter* Character : GetAliveCharacters())
	{
		Character->StopFire();
		Character->GetCharacterMovement()->DisableMovement();
	}
	for (AStrikeAIController* Bot : Bots)
	{
		if (Bot)
		{
			Bot->StopMovement();
		}
	}

	GetWorldTimerManager().SetTimer(PostMatchHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { RestartGame(); }), PostMatchDelay, false);
}

void AStrikeGameMode::FinishMatch(const FString& Text)
{
	if (IsMatchFinished())
	{
		return;
	}
	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->WinnerText = Text;
		GS->bMatchFinished = true;
	}
	UE_LOG(LogStrike, Log, TEXT("Fin de partie : %s"), *Text);
	EndMatch();
}

bool AStrikeGameMode::IsMatchFinished() const
{
	const AStrikeGameState* GS = GetStrikeGameState();
	return (GS && GS->bMatchFinished) || HasMatchEnded();
}

// ---------------------------------------------------------------------------
// Connexions / équipes / bots
// ---------------------------------------------------------------------------

void AStrikeGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	AssignTeam(NewPlayer, ChooseTeamFor(NewPlayer));
	if (IsMatchInProgress())
	{
		BalanceBots();
	}
}

void AStrikeGameMode::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	if (AStrikeAIController* Bot = Cast<AStrikeAIController>(Exiting))
	{
		Bots.Remove(Bot);
	}
	else if (IsMatchInProgress() && !IsMatchFinished())
	{
		// Un joueur humain quitte : un bot reprend sa place si le mode en utilise.
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, this, &AStrikeGameMode::BalanceBots, 0.5f, false);
	}
}

uint8 AStrikeGameMode::GetTeamOfController(const AController* Controller)
{
	if (!Controller)
	{
		return StrikeTeams::NoTeam;
	}
	if (const AStrikePlayerState* PS = Controller->GetPlayerState<AStrikePlayerState>())
	{
		return PS->GetTeamId();
	}
	if (const AStrikeAIController* AI = Cast<AStrikeAIController>(Controller))
	{
		return AI->GetTeamId();
	}
	return StrikeTeams::NoTeam;
}

FString AStrikeGameMode::GetDisplayName(const AController* Controller, const APawn* Pawn)
{
	if (const APlayerState* PS = Controller ? Controller->PlayerState.Get() : (Pawn ? Pawn->GetPlayerState() : nullptr))
	{
		return PS->GetPlayerName();
	}
	if (const AStrikeAIController* AI = Cast<AStrikeAIController>(Controller))
	{
		return AI->GetDisplayLabel();
	}
	return Pawn ? Pawn->GetClass()->GetName() : TEXT("Environnement");
}

int32 AStrikeGameMode::CountTeamMembers(uint8 Team) const
{
	int32 Count = 0;
	if (const AStrikeGameState* GS = GetStrikeGameState())
	{
		for (const AStrikePlayerState* PS : GS->GetStrikePlayers())
		{
			if (PS && PS->GetTeamId() == Team)
			{
				++Count;
			}
		}
	}
	return Count;
}

uint8 AStrikeGameMode::ChooseTeamFor(AController* Controller) const
{
	if (!bUseTeams)
	{
		// Chacun pour soi : une équipe unique par combattant.
		return static_cast<uint8>(NextFreeTeam++ % 200);
	}
	uint8 BestTeam = 0;
	int32 BestCount = TNumericLimits<int32>::Max();
	for (int32 Team = 0; Team < NumTeams; ++Team)
	{
		const int32 Count = CountTeamMembers(static_cast<uint8>(Team));
		if (Count < BestCount)
		{
			BestCount = Count;
			BestTeam = static_cast<uint8>(Team);
		}
	}
	return BestTeam;
}

void AStrikeGameMode::AssignTeam(AController* Controller, uint8 Team)
{
	if (AStrikePlayerState* PS = Controller ? Controller->GetPlayerState<AStrikePlayerState>() : nullptr)
	{
		PS->SetTeamId(Team);
	}
	if (AStrikeAIController* AI = Cast<AStrikeAIController>(Controller))
	{
		AI->SetTeamId(Team);
	}
	if (AStrikeCharacter* Character = Controller ? Cast<AStrikeCharacter>(Controller->GetPawn()) : nullptr)
	{
		Character->SetTeamId(Team);
	}
}

int32 AStrikeGameMode::GetDesiredBotCount() const
{
	if (TargetCombatants <= 0)
	{
		return 0;
	}
	return FMath::Max(0, TargetCombatants - GetNumPlayers());
}

void AStrikeGameMode::BalanceBots()
{
	if (!IsMatchInProgress() || IsMatchFinished())
	{
		return;
	}
	Bots.RemoveAll([](const TObjectPtr<AStrikeAIController>& Bot) { return !IsValid(Bot); });

	const int32 Desired = GetDesiredBotCount();
	while (Bots.Num() < Desired)
	{
		if (!SpawnBot())
		{
			break;
		}
	}
	while (Bots.Num() > Desired && Bots.Num() > 0)
	{
		RemoveBot(Bots.Last());
	}
}

AStrikeAIController* AStrikeGameMode::SpawnBot()
{
	if (!BotControllerClass)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStrikeAIController* Bot = GetWorld()->SpawnActor<AStrikeAIController>(BotControllerClass, Params);
	if (!Bot)
	{
		return nullptr;
	}

	static const TCHAR* BotNames[] = { TEXT("Viper"), TEXT("Ghost"), TEXT("Rook"), TEXT("Nova"), TEXT("Falcon"), TEXT("Echo"),
		TEXT("Raven"), TEXT("Titan"), TEXT("Blaze"), TEXT("Shadow"), TEXT("Kodiak"), TEXT("Orion"), TEXT("Jackal"), TEXT("Zulu") };
	const FString Name = FString::Printf(TEXT("[BOT] %s %d"), BotNames[BotNameCounter % static_cast<int32>(UE_ARRAY_COUNT(BotNames))], BotNameCounter / static_cast<int32>(UE_ARRAY_COUNT(BotNames)) + 1);
	++BotNameCounter;

	Bot->SetDifficulty(BotDifficulty);
	Bot->SetDisplayLabel(Name);
	if (APlayerState* PS = Bot->PlayerState)
	{
		PS->SetPlayerName(Name);
	}
	AssignTeam(Bot, ChooseTeamFor(Bot));
	Bots.Add(Bot);
	RestartPlayer(Bot);
	return Bot;
}

void AStrikeGameMode::RemoveBot(AStrikeAIController* Bot)
{
	Bots.Remove(Bot);
	if (IsValid(Bot))
	{
		if (APawn* Pawn = Bot->GetPawn())
		{
			Pawn->Destroy();
		}
		Bot->Destroy();
	}
}

// ---------------------------------------------------------------------------
// Apparition
// ---------------------------------------------------------------------------

UClass* AStrikeGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (Cast<AStrikeAIController>(InController) && BotPawnClass)
	{
		return BotPawnClass;
	}
	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

bool AStrikeGameMode::PlayerCanRestart_Implementation(APlayerController* Player)
{
	if (IsMatchFinished())
	{
		return false;
	}
	const AStrikePlayerState* PS = Player ? Player->GetPlayerState<AStrikePlayerState>() : nullptr;
	if (PS && (PS->IsEliminated() || PS->IsExtracted()))
	{
		return false;
	}
	return Super::PlayerCanRestart_Implementation(Player);
}

AActor* AStrikeGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const uint8 Team = GetTeamOfController(Player);
	const TArray<AStrikeCharacter*> Alive = GetAliveCharacters();

	TArray<AStrikeSpawnPoint*> Candidates;
	TArray<AStrikeSpawnPoint*> FreeCandidates;
	for (TActorIterator<AStrikeSpawnPoint> It(GetWorld()); It; ++It)
	{
		AStrikeSpawnPoint* Point = *It;
		if (Point->SpawnType != EStrikeSpawnType::Player)
		{
			continue;
		}
		if (bUseTeams && Point->TeamId == Team)
		{
			Candidates.Add(Point);
		}
		else if (Point->TeamId == StrikeTeams::NoTeam)
		{
			FreeCandidates.Add(Point);
		}
	}
	if (Candidates.Num() == 0)
	{
		Candidates = FreeCandidates;
	}
	if (Candidates.Num() == 0)
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	// Score : loin des ennemis vivants, jamais sur un point occupé.
	AStrikeSpawnPoint* Best = nullptr;
	float BestScore = -TNumericLimits<float>::Max();
	for (AStrikeSpawnPoint* Point : Candidates)
	{
		const FVector Location = Point->GetActorLocation();
		float NearestEnemy = 100000.f;
		bool bOccupied = false;
		for (const AStrikeCharacter* Character : Alive)
		{
			const float Dist = FVector::Dist(Character->GetActorLocation(), Location);
			if (Dist < 120.f)
			{
				bOccupied = true;
			}
			if (Character->GetTeamId() != Team || !bUseTeams)
			{
				NearestEnemy = FMath::Min(NearestEnemy, Dist);
			}
		}
		const float Score = (bOccupied ? -100000.f : 0.f) + FMath::Min(NearestEnemy, 6000.f) + FMath::FRandRange(0.f, 1500.f);
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Point;
		}
	}
	return Best;
}

void AStrikeGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);

	AStrikeCharacter* Character = Cast<AStrikeCharacter>(PlayerPawn);
	if (!Character)
	{
		return;
	}
	AController* Controller = Character->GetController();
	Character->SetTeamId(GetTeamOfController(Controller));
	ConfigureCharacter(Character, Controller);

	const TArray<TSubclassOf<AStrikeWeapon>> Loadout = GetLoadoutFor(Controller);
	for (int32 i = 0; i < Loadout.Num(); ++i)
	{
		Character->GetWeaponComponent()->GiveWeapon(Loadout[i], i == 0);
	}
}

void AStrikeGameMode::ConfigureCharacter(AStrikeCharacter* Character, AController* Controller)
{
	if (UHealthComponent* Health = Character->GetHealthComponent())
	{
		Health->bRegenerate = true;
	}
}

void AStrikeGameMode::RequestRespawn(AController* Controller, float Delay)
{
	if (!Controller)
	{
		return;
	}
	TWeakObjectPtr<AController> WeakController = Controller;
	FTimerHandle Handle;
	GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
	{
		AController* C = WeakController.Get();
		if (!C || C->GetPawn() || !IsMatchInProgress() || IsMatchFinished())
		{
			return;
		}
		if (APlayerController* PC = Cast<APlayerController>(C))
		{
			if (!PlayerCanRestart(PC))
			{
				return;
			}
		}
		RestartPlayer(C);
	}), FMath::Max(0.05f, Delay), false);
}

// ---------------------------------------------------------------------------
// Combat
// ---------------------------------------------------------------------------

bool AStrikeGameMode::CanDealDamage(AController* InstigatorController, AActor* Victim) const
{
	if (IsMatchFinished())
	{
		return false;
	}
	const AStrikeCharacter* VictimCharacter = Cast<AStrikeCharacter>(Victim);
	if (!InstigatorController || !VictimCharacter)
	{
		return true;
	}
	if (InstigatorController->GetPawn() == VictimCharacter)
	{
		return true; // dégâts auto-infligés (roquettes)
	}
	const uint8 InstigatorTeam = GetTeamOfController(InstigatorController);
	const uint8 VictimTeam = VictimCharacter->GetTeamId();
	if (InstigatorTeam == StrikeTeams::NoTeam || VictimTeam == StrikeTeams::NoTeam)
	{
		return true;
	}
	return InstigatorTeam != VictimTeam || bFriendlyFire;
}

void AStrikeGameMode::NotifyKilled(AController* Killer, AController* VictimController, APawn* VictimPawn, bool bHeadshot, AActor* DamageCauser)
{
	AStrikePlayerState* VictimPS = VictimController ? VictimController->GetPlayerState<AStrikePlayerState>() : nullptr;
	AStrikePlayerState* KillerPS = Killer ? Killer->GetPlayerState<AStrikePlayerState>() : nullptr;

	if (VictimPS)
	{
		VictimPS->AddDeath();
	}

	const uint8 KillerTeam = GetTeamOfController(Killer);
	const AStrikeCharacter* VictimCharacter = Cast<AStrikeCharacter>(VictimPawn);
	const uint8 VictimTeam = VictimCharacter ? VictimCharacter->GetTeamId() : GetTeamOfController(VictimController);
	const bool bSuicide = !Killer || Killer == VictimController;
	const bool bTeamKill = !bSuicide && bUseTeams && KillerTeam == VictimTeam;

	if (KillerPS && !bSuicide && !bTeamKill)
	{
		KillerPS->AddKill(bHeadshot);
	}

	FString WeaponName;
	if (const AStrikeWeapon* Weapon = Cast<AStrikeWeapon>(DamageCauser))
	{
		WeaponName = Weapon->WeaponName;
	}
	else if (const AStrikeProjectile* Projectile = Cast<AStrikeProjectile>(DamageCauser))
	{
		WeaponName = Projectile->GetWeaponName();
	}

	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		const FString KillerName = bSuicide ? FString() : GetDisplayName(Killer, Killer ? Killer->GetPawn() : nullptr);
		GS->MulticastKillFeed(KillerName, GetDisplayName(VictimController, VictimPawn), WeaponName, bHeadshot, KillerTeam, VictimTeam);
	}

	OnKill(bTeamKill ? nullptr : Killer, VictimController, VictimPawn, bHeadshot);

	if (VictimController && !IsMatchFinished())
	{
		if (ShouldRespawn(VictimController))
		{
			RequestRespawn(VictimController, RespawnDelay);
		}
		else
		{
			OnPlayerEliminated(VictimController);
		}
	}
}

void AStrikeGameMode::OnPlayerEliminated(AController* VictimController)
{
	if (AStrikePlayerState* PS = VictimController->GetPlayerState<AStrikePlayerState>())
	{
		PS->SetEliminated(true);
	}
	if (APlayerController* PC = Cast<APlayerController>(VictimController))
	{
		FTimerHandle Handle;
		TWeakObjectPtr<APlayerController> WeakPC = PC;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakPC]()
		{
			if (WeakPC.IsValid())
			{
				SpectateSomeone(WeakPC.Get());
			}
		}), 2.5f, false);
	}
}

void AStrikeGameMode::SpectateSomeone(APlayerController* PC)
{
	if (!PC || PC->GetPawn())
	{
		return;
	}
	const uint8 Team = GetTeamOfController(PC);
	TArray<AStrikeCharacter*> Alive = GetAliveCharacters(bUseTeams ? Team : StrikeTeams::NoTeam);
	if (Alive.Num() == 0)
	{
		Alive = GetAliveCharacters();
	}
	if (Alive.Num() > 0)
	{
		PC->SetViewTargetWithBlend(Alive[FMath::RandRange(0, Alive.Num() - 1)], 0.5f);
	}
}

TArray<AStrikeCharacter*> AStrikeGameMode::GetAliveCharacters(uint8 TeamFilter) const
{
	TArray<AStrikeCharacter*> Result;
	for (TActorIterator<AStrikeCharacter> It(GetWorld()); It; ++It)
	{
		AStrikeCharacter* Character = *It;
		if (IsValid(Character) && !Character->IsDead() && (TeamFilter == StrikeTeams::NoTeam || Character->GetTeamId() == TeamFilter))
		{
			Result.Add(Character);
		}
	}
	return Result;
}

TArray<AController*> AStrikeGameMode::GetCombatants() const
{
	TArray<AController*> Result;
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		if (AController* C = It->Get())
		{
			if (C->PlayerState)
			{
				Result.Add(C);
			}
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Chronomètre
// ---------------------------------------------------------------------------

void AStrikeGameMode::MatchTimerTick()
{
	if (!IsMatchInProgress() || IsMatchFinished())
	{
		return;
	}
	AStrikeGameState* GS = GetStrikeGameState();
	if (GS && MatchDuration > 0)
	{
		GS->RemainingTime = FMath::Max(0, GS->RemainingTime - 1);
		if (GS->RemainingTime == 0)
		{
			OnMatchTimeExpired();
			return;
		}
	}
	OnMatchTick();
}

void AStrikeGameMode::OnMatchTick()
{
	if (AStrikeGameState* GS = GetStrikeGameState())
	{
		GS->AliveCount = GetAliveCharacters().Num();
	}
}

void AStrikeGameMode::OnMatchTimeExpired()
{
	AStrikeGameState* GS = GetStrikeGameState();
	if (!GS)
	{
		FinishMatch(TEXT("Temps écoulé"));
		return;
	}

	if (bUseTeams && GS->TeamScores.Num() >= 2)
	{
		const int32 A = GS->TeamScores[0];
		const int32 B = GS->TeamScores[1];
		FinishMatch(A == B ? TEXT("Égalité !") : (A > B ? TEXT("Victoire de l'équipe BLEUE") : TEXT("Victoire de l'équipe ROUGE")));
		return;
	}

	const AStrikePlayerState* Best = nullptr;
	for (const AStrikePlayerState* PS : GS->GetStrikePlayers())
	{
		if (!Best || PS->GetKills() > Best->GetKills())
		{
			Best = PS;
		}
	}
	FinishMatch(Best ? FString::Printf(TEXT("Victoire : %s"), *Best->GetPlayerName()) : TEXT("Temps écoulé"));
}

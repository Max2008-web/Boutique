// STRIKE ZONE - IA de soldat (bots des modes compétitifs, ennemis d'Extraction)
#include "AI/StrikeAIController.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Weapons/StrikeWeapon.h"
#include "Game/StrikeGameState.h"
#include "StrikeZone.h"

#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationSystem.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"

AStrikeAIController::AStrikeAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
	// La rotation de contrôle est pilotée par la visée de l'IA, pas par l'orientation du pion.
	bSetControlRotationFromPawnOrientation = false;

	StrikePerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("StrikePerception"));

	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = 6000.f;
	SightConfig->LoseSightRadius = 6600.f;
	SightConfig->PeripheralVisionAngleDegrees = 75.f;
	SightConfig->SetMaxAge(5.f);
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = true;

	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = 4500.f;
	HearingConfig->SetMaxAge(4.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;

	StrikePerception->ConfigureSense(*SightConfig);
	StrikePerception->ConfigureSense(*HearingConfig);
	StrikePerception->SetDominantSense(SightConfig->GetSenseImplementation());
	SetPerceptionComponent(*StrikePerception);
}

AStrikeEnemyAIController::AStrikeEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = false;
	TeamId = StrikeTeams::Hostiles;
	DisplayLabel = TEXT("Mercenaire");
	bRoamTowardEnemies = false;
	PatrolRadius = 1500.f;
}

void AStrikeAIController::SetDifficulty(EStrikeAIDifficulty NewDifficulty)
{
	Difficulty = NewDifficulty;
	switch (Difficulty)
	{
	case EStrikeAIDifficulty::Easy:
		ReactionTime = 0.9f; AimErrorDegrees = 6.f; AimInterpSpeed = 4.f;
		BurstDuration = FVector2D(0.25f, 0.5f); BurstPause = FVector2D(0.9f, 1.5f);
		bUsesCover = false; StrafeChance = 0.2f;
		break;
	case EStrikeAIDifficulty::Hard:
		ReactionTime = 0.35f; AimErrorDegrees = 2.f; AimInterpSpeed = 11.f;
		BurstDuration = FVector2D(0.5f, 1.0f); BurstPause = FVector2D(0.3f, 0.7f);
		bUsesCover = true; StrafeChance = 0.7f;
		break;
	case EStrikeAIDifficulty::Veteran:
		ReactionTime = 0.2f; AimErrorDegrees = 1.f; AimInterpSpeed = 16.f;
		BurstDuration = FVector2D(0.6f, 1.2f); BurstPause = FVector2D(0.2f, 0.5f);
		bUsesCover = true; StrafeChance = 0.85f;
		break;
	default:
		ReactionTime = 0.55f; AimErrorDegrees = 3.5f; AimInterpSpeed = 7.f;
		BurstDuration = FVector2D(0.4f, 0.8f); BurstPause = FVector2D(0.5f, 1.0f);
		bUsesCover = true; StrafeChance = 0.5f;
		break;
	}
}

AStrikeCharacter* AStrikeAIController::GetStrikePawn() const
{
	return Cast<AStrikeCharacter>(GetPawn());
}

void AStrikeAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	StrikePerception->OnTargetPerceptionUpdated.RemoveDynamic(this, &AStrikeAIController::OnTargetPerceptionUpdated);
	StrikePerception->OnTargetPerceptionUpdated.AddDynamic(this, &AStrikeAIController::OnTargetPerceptionUpdated);
	InPawn->OnTakeAnyDamage.AddDynamic(this, &AStrikeAIController::OnPawnDamaged);

	if (!bHasHome)
	{
		HomeLocation = InPawn->GetActorLocation();
	}

	Target.Reset();
	bHasLastKnown = false;
	bFiring = false;
	bDirectMove = false;
	SetState(EStrikeBotState::Patrol);

	if (UCharacterMovementComponent* Move = InPawn->FindComponentByClass<UCharacterMovementComponent>())
	{
		Move->bUseRVOAvoidance = true;
		Move->AvoidanceConsiderationRadius = 300.f;
	}

	bUseBehaviorTree = BehaviorTreeAsset != nullptr && RunBehaviorTree(BehaviorTreeAsset);
}

void AStrikeAIController::OnUnPossess()
{
	StopShooting();
	if (APawn* OldPawn = GetPawn())
	{
		OldPawn->OnTakeAnyDamage.RemoveDynamic(this, &AStrikeAIController::OnPawnDamaged);
	}
	Super::OnUnPossess();
}

bool AStrikeAIController::IsEnemy(const AActor* Other) const
{
	const AStrikeCharacter* OtherCharacter = Cast<AStrikeCharacter>(Other);
	const AStrikeCharacter* Me = GetStrikePawn();
	if (!OtherCharacter || !Me || OtherCharacter == Me || OtherCharacter->IsDead())
	{
		return false;
	}
	return OtherCharacter->GetTeamId() != TeamId;
}

bool AStrikeAIController::IsMatchOver() const
{
	const AStrikeGameState* GS = GetWorld()->GetGameState<AStrikeGameState>();
	return GS && GS->bMatchFinished;
}

void AStrikeAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed() || !IsEnemy(Actor))
	{
		return;
	}
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		// Tir entendu : on va vérifier la position si on n'a pas mieux.
		if (!Target.IsValid())
		{
			LastKnownLocation = Stimulus.StimulusLocation;
			bHasLastKnown = true;
		}
	}
	else if (!Target.IsValid())
	{
		Target = Actor;
		TargetAcquiredTime = GetWorld()->GetTimeSeconds();
	}
}

void AStrikeAIController::OnPawnDamaged(AActor* DamagedActor, float Damage, const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	APawn* Attacker = InstigatedBy ? InstigatedBy->GetPawn() : nullptr;
	if (Attacker && IsEnemy(Attacker))
	{
		if (!Target.IsValid() || !bTargetVisible)
		{
			Target = Attacker;
			TargetAcquiredTime = GetWorld()->GetTimeSeconds();
		}
		LastKnownLocation = Attacker->GetActorLocation();
		bHasLastKnown = true;
	}
}

void AStrikeAIController::UpdateTargetSelection()
{
	const AStrikeCharacter* Me = GetStrikePawn();
	if (!Me)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	if (Target.IsValid() && !IsEnemy(Target.Get()))
	{
		Target.Reset();
	}

	// Choisit l'ennemi visible le plus proche parmi les acteurs perçus.
	TArray<AActor*> Perceived;
	StrikePerception->GetCurrentlyPerceivedActors(UAISense_Sight::StaticClass(), Perceived);
	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (AActor* Actor : Perceived)
	{
		if (!IsEnemy(Actor) || !LineOfSightTo(Actor))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), Me->GetActorLocation());
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Actor;
		}
	}
	if (Best && Best != Target.Get())
	{
		// Changement de cible : nouveau délai de réaction (sauf si l'ancienne n'était plus visible).
		if (!Target.IsValid() || !bTargetVisible || BestDistSq < FVector::DistSquared(Target->GetActorLocation(), Me->GetActorLocation()) * 0.5f)
		{
			Target = Best;
			TargetAcquiredTime = Now;
		}
	}

	bTargetVisible = Target.IsValid() && LineOfSightTo(Target.Get());
	if (bTargetVisible)
	{
		LastKnownLocation = Target->GetActorLocation();
		bHasLastKnown = true;
		LastSeenTime = Now;
	}
}

void AStrikeAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AStrikeCharacter* Me = GetStrikePawn();
	if (!Me || Me->IsDead())
	{
		StopShooting();
		return;
	}
	if (IsMatchOver())
	{
		StopShooting();
		StopMovement();
		return;
	}

	if (bUseBehaviorTree)
	{
		UpdateTargetSelection();
		UpdateBlackboard();
	}
	else
	{
		UpdateBrain(DeltaSeconds);
	}
	UpdateMovementFallback(DeltaSeconds);
}

void AStrikeAIController::UpdateBlackboard()
{
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsObject(TEXT("TargetActor"), Target.Get());
		BB->SetValueAsBool(TEXT("HasLineOfSight"), bTargetVisible);
		BB->SetValueAsVector(TEXT("LastKnownLocation"), LastKnownLocation);
		if (const AStrikeCharacter* Me = GetStrikePawn())
		{
			BB->SetValueAsFloat(TEXT("HealthFraction"), Me->GetHealthComponent()->GetHealthFraction());
		}
	}
}

void AStrikeAIController::SetState(EStrikeBotState NewState)
{
	if (State != NewState)
	{
		State = NewState;
		StateStartTime = GetWorld()->GetTimeSeconds();
		if (NewState != EStrikeBotState::TakeCover)
		{
			bHasCover = false;
		}
	}
}

// ---------------------------------------------------------------------------
// Cerveau natif
// ---------------------------------------------------------------------------

void AStrikeAIController::UpdateBrain(float DeltaSeconds)
{
	AStrikeCharacter* Me = GetStrikePawn();
	const float Now = GetWorld()->GetTimeSeconds();

	UpdateTargetSelection();

	const float HealthFraction = Me->GetHealthComponent()->GetHealthFraction();
	const float TimeInState = Now - StateStartTime;

	// --- Transitions ---
	if (Target.IsValid() && bTargetVisible)
	{
		if (bUsesCover && HealthFraction < 0.3f && State != EStrikeBotState::Retreat && State != EStrikeBotState::TakeCover)
		{
			SetState(EStrikeBotState::Retreat);
		}
		else if (bUsesCover && HealthFraction < 0.6f && State == EStrikeBotState::Engage && Now - LastCoverTime > 8.f)
		{
			SetState(EStrikeBotState::TakeCover);
		}
		else if (State == EStrikeBotState::Patrol || State == EStrikeBotState::Chase)
		{
			SetState(EStrikeBotState::Engage);
		}
	}
	else if (Target.IsValid() || bHasLastKnown)
	{
		if (State == EStrikeBotState::Engage || State == EStrikeBotState::Patrol)
		{
			SetState(EStrikeBotState::Chase);
		}
		if (Now - LastSeenTime > 10.f && State == EStrikeBotState::Chase && TimeInState > 6.f)
		{
			Target.Reset();
			bHasLastKnown = false;
			SetState(EStrikeBotState::Patrol);
		}
	}
	else if (State != EStrikeBotState::Patrol)
	{
		SetState(EStrikeBotState::Patrol);
	}

	// --- Actions ---
	switch (State)
	{
	case EStrikeBotState::Engage:
	{
		AimAndShoot(DeltaSeconds);
		if (Now >= NextStrafeTime)
		{
			NextStrafeTime = Now + FMath::FRandRange(1.2f, 2.8f);
			if (FMath::FRand() < StrafeChance)
			{
				const FVector Side = Me->GetActorRightVector() * (FMath::RandBool() ? 1.f : -1.f) * FMath::FRandRange(250.f, 600.f);
				MoveToPoint(Me->GetActorLocation() + Side, 60.f);
			}
			else
			{
				StopMovement();
				bDirectMove = false;
			}
		}
		break;
	}

	case EStrikeBotState::TakeCover:
	{
		if (!bHasCover && !bWaitingForEQS)
		{
			if (CoverQuery)
			{
				bWaitingForEQS = true;
				FEnvQueryRequest Request(CoverQuery, Me);
				Request.Execute(EEnvQueryRunMode::SingleResult, FQueryFinishedSignature::CreateUObject(this, &AStrikeAIController::OnCoverQueryFinished));
			}
			else if (FindCoverLocation(CoverLocation))
			{
				bHasCover = true;
				MoveToPoint(CoverLocation, 50.f);
			}
			else
			{
				LastCoverTime = Now;
				SetState(EStrikeBotState::Engage);
			}
		}
		if (bTargetVisible && Difficulty >= EStrikeAIDifficulty::Hard)
		{
			AimAndShoot(DeltaSeconds); // tir de couverture en se déplaçant
		}
		else
		{
			StopShooting();
		}
		if (bHasCover && IsMoveFinished())
		{
			// À couvert : recharge, s'accroupit, puis ressort.
			Me->ReloadWeapon();
			if (!Me->bIsCrouched)
			{
				Me->StartCrouchOrSlide();
			}
			if (TimeInState > 3.5f)
			{
				Me->StopCrouchOrSlide();
				LastCoverTime = Now;
				SetState(EStrikeBotState::Engage);
			}
		}
		if (TimeInState > 8.f)
		{
			Me->StopCrouchOrSlide();
			LastCoverTime = Now;
			SetState(EStrikeBotState::Engage);
		}
		break;
	}

	case EStrikeBotState::Retreat:
	{
		if (TimeInState < 0.1f && Target.IsValid())
		{
			const FVector Away = (Me->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal2D();
			MoveToPoint(Me->GetActorLocation() + Away * 1200.f, 100.f);
		}
		if (bTargetVisible)
		{
			AimAndShoot(DeltaSeconds);
		}
		if (TimeInState > 4.f || (IsMoveFinished() && TimeInState > 1.f))
		{
			LastCoverTime = Now;
			SetState(Target.IsValid() ? EStrikeBotState::TakeCover : EStrikeBotState::Patrol);
		}
		break;
	}

	case EStrikeBotState::Chase:
	{
		StopShooting();
		if (TimeInState < 0.1f || IsMoveFinished())
		{
			if (FVector::Dist2D(Me->GetActorLocation(), LastKnownLocation) < 200.f)
			{
				bHasLastKnown = false;
				Target.Reset();
				SetState(EStrikeBotState::Patrol);
			}
			else
			{
				MoveToPoint(LastKnownLocation, 120.f);
			}
		}
		FaceMovementDirection(DeltaSeconds);
		break;
	}

	case EStrikeBotState::Patrol:
	default:
	{
		StopShooting();
		if (IsMoveFinished() && TimeInState > 0.5f)
		{
			ChooseRoamDestination();
			StateStartTime = Now;
		}
		FaceMovementDirection(DeltaSeconds);
		break;
	}
	}

	// Interaction maintenue sur l'objectif (pose / désamorçage) quand aucun ennemi n'est visible.
	if (bHasObjective && bInteractAtObjective)
	{
		const bool bAtObjective = !bTargetVisible && FVector::Dist2D(Me->GetActorLocation(), ObjectiveLocation) < 300.f;
		Me->SetInteractHeld(bAtObjective);
	}

	// Recharge opportuniste hors combat.
	if (!bTargetVisible)
	{
		if (const AStrikeWeapon* Weapon = Me->GetCurrentWeapon())
		{
			if (Weapon->GetCurrentAmmo() < Weapon->MagazineSize / 2)
			{
				Me->ReloadWeapon();
			}
		}
	}
}

void AStrikeAIController::AimAndShoot(float DeltaSeconds)
{
	AStrikeCharacter* Me = GetStrikePawn();
	AActor* TargetActor = Target.Get();
	if (!Me || !TargetActor)
	{
		StopShooting();
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	// Erreur de visée qui dérive lentement ; plus grande si le bot ou la cible bouge.
	if (Now >= NextAimErrorTime)
	{
		NextAimErrorTime = Now + FMath::FRandRange(0.35f, 0.7f);
		const float MovingFactor = (Me->GetVelocity().Size2D() > 50.f ? 1.4f : 1.f) * (TargetActor->GetVelocity().Size2D() > 50.f ? 1.3f : 1.f);
		const float Error = AimErrorDegrees * MovingFactor;
		AimError = FRotator(FMath::FRandRange(-Error, Error) * 0.6f, FMath::FRandRange(-Error, Error), 0.f);
	}

	// Vise le torse, parfois la tête pour les difficultés élevées.
	const float AimHeight = (Difficulty >= EStrikeAIDifficulty::Hard && FMath::Frac(Now * 0.37f) > 0.7f) ? 65.f : 25.f;
	const FVector AimPoint = TargetActor->GetActorLocation() + FVector(0.f, 0.f, AimHeight);
	const FVector Eye = Me->GetPawnViewLocation();
	const FRotator Desired = (AimPoint - Eye).Rotation() + AimError;
	const FRotator NewRotation = FMath::RInterpTo(GetControlRotation(), Desired, DeltaSeconds, AimInterpSpeed);
	SetControlRotation(NewRotation);

	if (bPassive)
	{
		StopShooting();
		return;
	}

	// Temps de réaction avant le premier tir.
	if (Now - TargetAcquiredTime < ReactionTime || !bTargetVisible)
	{
		StopShooting();
		return;
	}

	const AStrikeWeapon* Weapon = Me->GetCurrentWeapon();
	if (Weapon && Weapon->GetCurrentAmmo() <= 0)
	{
		StopShooting();
		Me->ReloadWeapon();
		return;
	}

	const float AngleToTarget = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(NewRotation.Vector(), (AimPoint - Eye).GetSafeNormal()), -1.f, 1.f)));
	if (!bFiring && Now >= NextBurstTime && AngleToTarget < 8.f)
	{
		bFiring = true;
		BurstEndTime = Now + FMath::FRandRange(BurstDuration.X, BurstDuration.Y);
		Me->StartFire();
	}
	else if (bFiring && Now >= BurstEndTime)
	{
		bFiring = false;
		NextBurstTime = Now + FMath::FRandRange(BurstPause.X, BurstPause.Y);
		Me->StopFire();
	}
	else if (bFiring && Weapon && !Weapon->bAutomatic)
	{
		// Armes semi-automatiques : relâche/presse la détente à chaque coup.
		Me->StopFire();
		Me->StartFire();
	}
}

void AStrikeAIController::StopShooting()
{
	if (bFiring)
	{
		bFiring = false;
		if (AStrikeCharacter* Me = GetStrikePawn())
		{
			Me->StopFire();
		}
	}
}

bool AStrikeAIController::FindCoverLocation(FVector& OutLocation) const
{
	const AStrikeCharacter* Me = GetStrikePawn();
	const AActor* Threat = Target.Get();
	if (!Me || !Threat)
	{
		return false;
	}

	const FVector MyLocation = Me->GetActorLocation();
	FVector ThreatEye = Threat->GetActorLocation() + FVector(0.f, 0.f, 60.f);
	if (const APawn* ThreatPawn = Cast<APawn>(Threat))
	{
		ThreatEye = ThreatPawn->GetPawnViewLocation();
	}

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StrikeCover), false, Me);
	Params.AddIgnoredActor(Threat);

	float BestScore = -TNumericLimits<float>::Max();
	bool bFound = false;
	for (int32 i = 0; i < 20; ++i)
	{
		const float Angle = (i / 20.f) * 2.f * PI;
		const float Radius = FMath::FRandRange(350.f, 1300.f);
		FVector Candidate = MyLocation + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

		if (NavSys)
		{
			FNavLocation NavLocation;
			if (!NavSys->ProjectPointToNavigation(Candidate, NavLocation, FVector(150.f, 150.f, 300.f)))
			{
				continue;
			}
			Candidate = NavLocation.Location + FVector(0.f, 0.f, 50.f);
		}

		const float ThreatDistance = FVector::Dist2D(Candidate, Threat->GetActorLocation());
		if (ThreatDistance < 700.f)
		{
			continue;
		}

		// Couvert si la ligne de vue menace -> position accroupie est bloquée.
		FHitResult Hit;
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit, ThreatEye, Candidate + FVector(0.f, 0.f, 10.f), ECC_Visibility, Params);
		if (!bBlocked)
		{
			continue;
		}

		const float Score = -FVector::Dist2D(Candidate, MyLocation) + ThreatDistance * 0.3f;
		if (Score > BestScore)
		{
			BestScore = Score;
			OutLocation = Candidate;
			bFound = true;
		}
	}
	return bFound;
}

void AStrikeAIController::OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	bWaitingForEQS = false;
	if (Result.IsValid() && Result->IsSuccessful())
	{
		CoverLocation = Result->GetItemAsLocation(0);
		bHasCover = true;
		MoveToPoint(CoverLocation, 50.f);
	}
	else
	{
		LastCoverTime = GetWorld()->GetTimeSeconds();
		SetState(EStrikeBotState::Engage);
	}
}

void AStrikeAIController::SetObjective(const FVector& Location, bool bInteract)
{
	const bool bChanged = !bHasObjective || FVector::DistSquared(Location, ObjectiveLocation) > FMath::Square(100.f);
	bHasObjective = true;
	bInteractAtObjective = bInteract;
	ObjectiveLocation = Location;
	if (bChanged && State == EStrikeBotState::Patrol)
	{
		ChooseRoamDestination();
	}
}

void AStrikeAIController::ClearObjective()
{
	bHasObjective = false;
	bInteractAtObjective = false;
	if (AStrikeCharacter* Me = GetStrikePawn())
	{
		Me->SetInteractHeld(false);
	}
}

void AStrikeAIController::ChooseRoamDestination()
{
	const AStrikeCharacter* Me = GetStrikePawn();
	if (!Me)
	{
		return;
	}

	// Objectif de mode prioritaire (sites tactiques, zone du Battle Royale).
	if (bHasObjective)
	{
		const FVector Offset(FMath::FRandRange(-150.f, 150.f), FMath::FRandRange(-150.f, 150.f), 0.f);
		MoveToPoint(ObjectiveLocation + Offset, 80.f);
		return;
	}

	// Modes compétitifs : se dirige vers un ennemi connu (renseignement d'escouade approximatif).
	if (bRoamTowardEnemies)
	{
		TArray<const AStrikeCharacter*> Enemies;
		for (TActorIterator<AStrikeCharacter> It(GetWorld()); It; ++It)
		{
			if (IsEnemy(*It))
			{
				Enemies.Add(*It);
			}
		}
		if (Enemies.Num() > 0)
		{
			const AStrikeCharacter* Pick = Enemies[FMath::RandRange(0, Enemies.Num() - 1)];
			const FVector Offset(FMath::FRandRange(-900.f, 900.f), FMath::FRandRange(-900.f, 900.f), 0.f);
			MoveToPoint(Pick->GetActorLocation() + Offset, 150.f);
			return;
		}
	}

	const FVector Origin = bHasHome ? HomeLocation : Me->GetActorLocation();
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Result;
		if (NavSys->GetRandomReachablePointInRadius(Origin, PatrolRadius, Result))
		{
			MoveToPoint(Result.Location, 120.f);
			return;
		}
	}
	MoveToPoint(Origin + FVector(FMath::FRandRange(-PatrolRadius, PatrolRadius), FMath::FRandRange(-PatrolRadius, PatrolRadius), 0.f), 150.f);
}

// ---------------------------------------------------------------------------
// Déplacements : NavMesh si disponible, sinon pilotage direct
// ---------------------------------------------------------------------------

void AStrikeAIController::MoveToPoint(const FVector& Destination, float AcceptanceRadius)
{
	const EPathFollowingRequestResult::Type Result = MoveToLocation(Destination, AcceptanceRadius, true, true, true, true, nullptr, true);
	bDirectMove = (Result == EPathFollowingRequestResult::Failed);
	if (bDirectMove)
	{
		DirectDestination = Destination;
		DirectAcceptance = FMath::Max(AcceptanceRadius, 80.f);
		StuckTime = 0.f;
	}
}

void AStrikeAIController::MoveToTargetActor(AActor* Goal, float AcceptanceRadius)
{
	if (!Goal)
	{
		return;
	}
	const EPathFollowingRequestResult::Type Result = MoveToActor(Goal, AcceptanceRadius, true, true, true, nullptr, true);
	bDirectMove = (Result == EPathFollowingRequestResult::Failed);
	if (bDirectMove)
	{
		DirectDestination = Goal->GetActorLocation();
		DirectAcceptance = FMath::Max(AcceptanceRadius, 80.f);
	}
}

bool AStrikeAIController::IsMoveFinished() const
{
	if (bDirectMove)
	{
		return false;
	}
	return GetMoveStatus() == EPathFollowingStatus::Idle;
}

void AStrikeAIController::UpdateMovementFallback(float DeltaSeconds)
{
	if (!bDirectMove)
	{
		return;
	}
	AStrikeCharacter* Me = GetStrikePawn();
	if (!Me)
	{
		return;
	}

	const FVector Location = Me->GetActorLocation();
	FVector ToGoal = DirectDestination - Location;
	ToGoal.Z = 0.f;
	if (ToGoal.Size() <= DirectAcceptance)
	{
		bDirectMove = false;
		return;
	}

	FVector Direction = ToGoal.GetSafeNormal();

	// Évitement simple : si un obstacle bloque, contourne par un côté.
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(StrikeAvoid), false, Me);
	const FVector Start = Location - FVector(0.f, 0.f, 40.f);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * 180.f, ECC_Visibility, Params))
	{
		Direction = Direction.RotateAngleAxis(70.f * AvoidSign, FVector::UpVector);
	}

	Me->AddMovementInput(Direction, 1.f);

	// Détection de blocage.
	if (FVector::DistSquared2D(Location, LastPawnLocation) < FMath::Square(20.f * DeltaSeconds * 10.f))
	{
		StuckTime += DeltaSeconds;
		if (StuckTime > 1.5f)
		{
			StuckTime = 0.f;
			AvoidSign *= -1.f;
			if (FMath::FRand() < 0.3f)
			{
				bDirectMove = false; // abandonne, une nouvelle destination sera choisie
			}
		}
	}
	else
	{
		StuckTime = 0.f;
	}
	LastPawnLocation = Location;
}

void AStrikeAIController::FaceMovementDirection(float DeltaSeconds)
{
	const APawn* Me = GetPawn();
	if (!Me)
	{
		return;
	}
	const FVector Velocity = Me->GetVelocity();
	if (Velocity.SizeSquared2D() > 400.f)
	{
		const FRotator Desired(0.f, Velocity.Rotation().Yaw, 0.f);
		SetControlRotation(FMath::RInterpTo(GetControlRotation(), Desired, DeltaSeconds, 6.f));
	}
}

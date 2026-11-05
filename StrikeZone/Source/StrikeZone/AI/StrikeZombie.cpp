// STRIKE ZONE - zombies (personnage + IA de horde distincte de celle des soldats)
#include "AI/StrikeZombie.h"
#include "Character/StrikeCharacterMovement.h"
#include "Components/HealthComponent.h"

#include "Animation/AnimMontage.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

AStrikeZombie::AStrikeZombie(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	AIControllerClass = AStrikeZombieAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	TeamId = StrikeTeams::Zombies;
	HealthComponent->bRegenerate = false;
	HealthComponent->MaxHealth = 100.f;
}

void AStrikeZombie::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeZombie, ZombieType);
}

void AStrikeZombie::InitZombie(EStrikeZombieType InType, float HealthMultiplier)
{
	ZombieType = InType;

	float BaseHealth = 100.f;
	float Speed = 260.f;
	switch (ZombieType)
	{
	case EStrikeZombieType::Runner:
		BaseHealth = 70.f; Speed = 560.f; AttackDamage = 18.f; AttackCooldown = 0.9f;
		break;
	case EStrikeZombieType::Brute:
		BaseHealth = 350.f; Speed = 240.f; AttackDamage = 45.f; AttackCooldown = 1.6f; AttackRange = 190.f;
		break;
	case EStrikeZombieType::Boss:
		BaseHealth = 2500.f; Speed = 330.f; AttackDamage = 70.f; AttackCooldown = 1.4f; AttackRange = 260.f;
		break;
	default:
		break;
	}

	HealthComponent->SetMaxHealth(BaseHealth * HealthMultiplier, true);
	GetCharacterMovement()->MaxWalkSpeed = Speed;
	ApplyTypeVisuals();
}

void AStrikeZombie::OnRep_ZombieType()
{
	ApplyTypeVisuals();
}

void AStrikeZombie::ApplyTypeVisuals()
{
	float Scale = 1.f;
	switch (ZombieType)
	{
	case EStrikeZombieType::Runner: Scale = 0.9f; break;
	case EStrikeZombieType::Brute: Scale = 1.3f; break;
	case EStrikeZombieType::Boss: Scale = 1.9f; break;
	default: break;
	}
	SetActorScale3D(FVector(Scale));
}

bool AStrikeZombie::TryAttack(AActor* Victim)
{
	const float Now = GetWorld()->GetTimeSeconds();
	if (!HasAuthority() || IsDead() || !Victim || Now < NextAttackTime)
	{
		return false;
	}
	if (FVector::Dist(Victim->GetActorLocation(), GetActorLocation()) > AttackRange + 60.f)
	{
		return false;
	}
	NextAttackTime = Now + AttackCooldown;
	UGameplayStatics::ApplyDamage(Victim, AttackDamage, GetController(), this, UDamageType::StaticClass());
	MulticastAttack();
	return true;
}

void AStrikeZombie::MulticastAttack_Implementation()
{
	if (AttackMontage)
	{
		PlayAnimMontage(AttackMontage);
	}
}

// ===========================================================================
// AStrikeZombieAIController
// ===========================================================================

AStrikeZombieAIController::AStrikeZombieAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bWantsPlayerState = false;
	TeamId = StrikeTeams::Zombies;
	DisplayLabel = TEXT("Zombie");
	bRoamTowardEnemies = true;
}

void AStrikeZombieAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (const AStrikeZombie* Zombie = Cast<AStrikeZombie>(InPawn))
	{
		switch (Zombie->GetZombieType())
		{
		case EStrikeZombieType::Runner: DisplayLabel = TEXT("Coureur"); break;
		case EStrikeZombieType::Brute: DisplayLabel = TEXT("Brute"); break;
		case EStrikeZombieType::Boss: DisplayLabel = TEXT("BOSS"); break;
		default: DisplayLabel = TEXT("Zombie"); break;
		}
	}
}

void AStrikeZombieAIController::UpdateBrain(float DeltaSeconds)
{
	AStrikeZombie* Zombie = Cast<AStrikeZombie>(GetPawn());
	if (!Zombie)
	{
		return;
	}
	const float Now = GetWorld()->GetTimeSeconds();

	// La horde "sent" le survivant le plus proche (pas besoin de ligne de vue).
	if (Now >= NextRetargetTime || !Target.IsValid() || !IsEnemy(Target.Get()))
	{
		NextRetargetTime = Now + 1.f;
		AActor* Best = nullptr;
		float BestDistSq = TNumericLimits<float>::Max();
		for (TActorIterator<AStrikeCharacter> It(GetWorld()); It; ++It)
		{
			if (IsEnemy(*It))
			{
				const float DistSq = FVector::DistSquared(It->GetActorLocation(), Zombie->GetActorLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Best = *It;
				}
			}
		}
		if (Best != Target.Get())
		{
			Target = Best;
			NextRepathTime = 0.f;
		}
	}

	AActor* Victim = Target.Get();
	if (!Victim)
	{
		StopMovement();
		return;
	}

	const float Distance = FVector::Dist(Victim->GetActorLocation(), Zombie->GetActorLocation());
	const FRotator ToVictim(0.f, (Victim->GetActorLocation() - Zombie->GetActorLocation()).Rotation().Yaw, 0.f);

	if (Distance <= Zombie->GetAttackRange())
	{
		StopMovement();
		bDirectMove = false;
		SetControlRotation(FMath::RInterpTo(GetControlRotation(), ToVictim, DeltaSeconds, 10.f));
		Zombie->TryAttack(Victim);
		return;
	}

	if (Now >= NextRepathTime)
	{
		NextRepathTime = Now + 0.5f;
		MoveToTargetActor(Victim, Zombie->GetAttackRange() * 0.5f);
	}
	if (bDirectMove)
	{
		DirectDestination = Victim->GetActorLocation();
	}
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), ToVictim, DeltaSeconds, 6.f));
}

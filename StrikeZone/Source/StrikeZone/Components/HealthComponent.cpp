// STRIKE ZONE - santé, armure, dégâts localisés (autorité serveur)
#include "Components/HealthComponent.h"
#include "Game/StrikeGameMode.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;
	SetIsReplicatedByDefault(true);
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	if (GetOwner() && GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
		Armor = FMath::Clamp(StartingArmor, 0.f, MaxArmor);
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UHealthComponent, Health);
	DOREPLIFETIME(UHealthComponent, Armor);
	DOREPLIFETIME(UHealthComponent, bIsDead);
}

void UHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetOwner()->HasAuthority() || bIsDead || !bRegenerate || Health >= MaxHealth)
	{
		return;
	}
	if (GetWorld()->GetTimeSeconds() - LastDamageTime >= RegenDelay)
	{
		Heal(RegenPerSecond * DeltaTime);
	}
}

float UHealthComponent::ApplyDamage(float Amount, AController* InstigatorController, AActor* DamageCauser, EStrikeHitZone Zone, const FVector& SourceLocation)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bIsDead || Amount <= 0.f)
	{
		return 0.f;
	}

	AStrikeGameMode* GameMode = GetWorld()->GetAuthGameMode<AStrikeGameMode>();
	if (GameMode)
	{
		if (!GameMode->CanDealDamage(InstigatorController, Owner))
		{
			return 0.f;
		}
		Amount = GameMode->ModifyDamage(Amount, InstigatorController, Owner);
	}

	// Armure : absorbe une partie des dégâts (sauf tir à la tête qui l'ignore partiellement).
	float ToHealth = Amount;
	if (Armor > 0.f)
	{
		const float Absorption = (Zone == EStrikeHitZone::Head) ? ArmorAbsorption * 0.5f : ArmorAbsorption;
		const float Absorbed = FMath::Min(Armor, Amount * Absorption);
		Armor -= Absorbed;
		ToHealth -= Absorbed;
	}

	const float OldHealth = Health;
	Health = FMath::Clamp(Health - ToHealth, 0.f, MaxHealth);
	LastDamageTime = GetWorld()->GetTimeSeconds();

	ClientNotifyDamaged(SourceLocation);
	OnHealthChanged.Broadcast(this, Health, Armor);

	const float Dealt = OldHealth - Health;
	if (GameMode)
	{
		GameMode->NotifyDamageDealt(InstigatorController, Owner, Dealt, Zone);
	}

	if (Health <= 0.f)
	{
		bIsDead = true;
		OnDeath.Broadcast(this, InstigatorController, DamageCauser, Zone == EStrikeHitZone::Head);
	}
	return Dealt;
}

void UHealthComponent::Heal(float Amount)
{
	if (!GetOwner()->HasAuthority() || bIsDead || Amount <= 0.f)
	{
		return;
	}
	Health = FMath::Min(MaxHealth, Health + Amount);
	OnHealthChanged.Broadcast(this, Health, Armor);
}

void UHealthComponent::AddArmor(float Amount)
{
	if (!GetOwner()->HasAuthority() || bIsDead)
	{
		return;
	}
	Armor = FMath::Clamp(Armor + Amount, 0.f, MaxArmor);
	OnHealthChanged.Broadcast(this, Health, Armor);
}

void UHealthComponent::Kill(AController* Killer)
{
	if (!GetOwner()->HasAuthority() || bIsDead)
	{
		return;
	}
	Health = 0.f;
	bIsDead = true;
	OnHealthChanged.Broadcast(this, Health, Armor);
	OnDeath.Broadcast(this, Killer, nullptr, false);
}

void UHealthComponent::ResetHealth()
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	bIsDead = false;
	Health = MaxHealth;
	Armor = FMath::Clamp(StartingArmor, 0.f, MaxArmor);
	OnHealthChanged.Broadcast(this, Health, Armor);
}

void UHealthComponent::SetMaxHealth(float NewMax, bool bFill)
{
	MaxHealth = FMath::Max(1.f, NewMax);
	if (bFill && GetOwner() && GetOwner()->HasAuthority())
	{
		Health = MaxHealth;
	}
}

void UHealthComponent::OnRep_Health()
{
	OnHealthChanged.Broadcast(this, Health, Armor);
}

void UHealthComponent::OnRep_IsDead()
{
	if (bIsDead)
	{
		OnDeath.Broadcast(this, nullptr, nullptr, false);
	}
}

void UHealthComponent::ClientNotifyDamaged_Implementation(FVector_NetQuantize SourceLocation)
{
	LastDamageSource = SourceLocation;
	LastDamageClientTime = GetWorld()->GetTimeSeconds();
}

EStrikeHitZone UHealthComponent::GetHitZoneFromHit(const FHitResult& Hit, const AActor* Victim)
{
	// 1) Par os si un Skeletal Mesh a été touché.
	if (Hit.BoneName != NAME_None)
	{
		const FString Bone = Hit.BoneName.ToString().ToLower();
		if (Bone.Contains(TEXT("head")) || Bone.Contains(TEXT("neck")))
		{
			return EStrikeHitZone::Head;
		}
		if (Bone.Contains(TEXT("arm")) || Bone.Contains(TEXT("hand")) || Bone.Contains(TEXT("clavicle")))
		{
			return EStrikeHitZone::Arms;
		}
		if (Bone.Contains(TEXT("thigh")) || Bone.Contains(TEXT("calf")) || Bone.Contains(TEXT("foot")) || Bone.Contains(TEXT("leg")))
		{
			return EStrikeHitZone::Legs;
		}
		return EStrikeHitZone::Torso;
	}

	// 2) Composant marqué "Head".
	if (const UPrimitiveComponent* HitComp = Hit.GetComponent())
	{
		if (HitComp->ComponentHasTag(TEXT("Head")))
		{
			return EStrikeHitZone::Head;
		}
	}

	// 3) Hauteur relative sur la capsule (corps temporaires).
	if (const ACharacter* Character = Cast<ACharacter>(Victim))
	{
		const UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const float Relative = (Hit.ImpactPoint.Z - Capsule->GetComponentLocation().Z) / FMath::Max(HalfHeight, 1.f);
		if (Relative > 0.68f)
		{
			return EStrikeHitZone::Head;
		}
		if (Relative < -0.3f)
		{
			return EStrikeHitZone::Legs;
		}
		// Bras : impact latéral éloigné de l'axe du corps.
		const FVector Local = Capsule->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
		if (FMath::Abs(Local.Y) > Capsule->GetScaledCapsuleRadius() * 0.75f)
		{
			return EStrikeHitZone::Arms;
		}
	}
	return EStrikeHitZone::Torso;
}

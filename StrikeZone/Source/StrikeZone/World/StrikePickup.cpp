// STRIKE ZONE - objets ramassables (armes, munitions, armure, soins, butin)
#include "World/StrikePickup.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Components/WeaponComponent.h"
#include "Weapons/StrikeWeapon.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AStrikePickup::AStrikePickup()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = true;
	SetNetUpdateFrequency(2.f);
	SetNetCullDistanceSquared(FMath::Square(12000.f));

	Trigger = CreateDefaultSubobject<USphereComponent>(TEXT("Trigger"));
	Trigger->InitSphereRadius(90.f);
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Trigger->SetCanEverAffectNavigation(false);
	RootComponent = Trigger;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Trigger);
	Mesh->SetRelativeScale3D(FVector(0.35f));
	// Bloque uniquement la visibilité : sert à la détection d'interaction, pas aux tirs ni aux déplacements.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetCanEverAffectNavigation(false);
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AStrikePickup::BeginPlay()
{
	Super::BeginPlay();
	BaseZ = Mesh->GetRelativeLocation().Z;
	MeshMaterial = Mesh->CreateDynamicMaterialInstance(0);
	RefreshVisual();
	if (HasAuthority())
	{
		Trigger->OnComponentBeginOverlap.AddDynamic(this, &AStrikePickup::OnTriggerOverlap);
	}
}

void AStrikePickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikePickup, PickupType);
	DOREPLIFETIME(AStrikePickup, WeaponClass);
	DOREPLIFETIME(AStrikePickup, AmmoType);
	DOREPLIFETIME(AStrikePickup, Amount);
	DOREPLIFETIME(AStrikePickup, LootItem);
	DOREPLIFETIME(AStrikePickup, bActive);
}

void AStrikePickup::SetupWeapon(TSubclassOf<AStrikeWeapon> InWeaponClass)
{
	PickupType = EStrikePickupType::Weapon;
	WeaponClass = InWeaponClass;
	RefreshVisual();
}

void AStrikePickup::SetupAmmo(EStrikeAmmoType InAmmoType, int32 InAmount)
{
	PickupType = EStrikePickupType::Ammo;
	AmmoType = InAmmoType;
	Amount = InAmount;
	RefreshVisual();
}

void AStrikePickup::SetupArmor(int32 InAmount)
{
	PickupType = EStrikePickupType::Armor;
	Amount = InAmount;
	RefreshVisual();
}

void AStrikePickup::SetupHealth(int32 InAmount)
{
	PickupType = EStrikePickupType::Health;
	Amount = InAmount;
	RefreshVisual();
}

void AStrikePickup::SetupLoot(const FStrikeLootItem& InItem)
{
	PickupType = EStrikePickupType::Loot;
	LootItem = InItem;
	RefreshVisual();
}

void AStrikePickup::OnRep_Config()
{
	RefreshVisual();
}

void AStrikePickup::RefreshVisual()
{
	FLinearColor Color = FLinearColor::White;
	FVector Scale(0.35f);
	switch (PickupType)
	{
	case EStrikePickupType::Weapon: Color = FLinearColor(0.9f, 0.9f, 0.9f); Scale = FVector(0.7f, 0.12f, 0.18f); break;
	case EStrikePickupType::Ammo: Color = FLinearColor(0.9f, 0.75f, 0.1f); Scale = FVector(0.3f, 0.2f, 0.2f); break;
	case EStrikePickupType::Armor: Color = FLinearColor(0.1f, 0.5f, 1.f); Scale = FVector(0.35f, 0.35f, 0.15f); break;
	case EStrikePickupType::Health: Color = FLinearColor(0.1f, 0.9f, 0.2f); Scale = FVector(0.3f); break;
	case EStrikePickupType::Loot: Color = FLinearColor(0.75f, 0.3f, 1.f); Scale = FVector(0.25f, 0.25f, 0.35f); break;
	}
	Mesh->SetRelativeScale3D(Scale);
	if (MeshMaterial)
	{
		MeshMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

void AStrikePickup::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer || !bActive)
	{
		return;
	}
	const float Time = GetWorld()->GetTimeSeconds();
	Mesh->SetRelativeLocationAndRotation(FVector(0.f, 0.f, BaseZ + FMath::Sin(Time * 2.f) * 10.f), FRotator(0.f, Time * 60.f, 0.f));
}

bool AStrikePickup::CanInteract(const AStrikeCharacter* User) const
{
	return bActive && User && !User->IsDead() && PickupType == EStrikePickupType::Weapon && WeaponClass;
}

FString AStrikePickup::GetInteractPrompt(const AStrikeCharacter* User) const
{
	const AStrikeWeapon* CDO = WeaponClass ? WeaponClass->GetDefaultObject<AStrikeWeapon>() : nullptr;
	return FString::Printf(TEXT("Prendre %s"), CDO ? *CDO->WeaponName : TEXT("l'arme"));
}

void AStrikePickup::Interact(AStrikeCharacter* User)
{
	if (HasAuthority() && CanInteract(User) && GiveTo(User))
	{
		SetActive(false);
	}
}

void AStrikePickup::OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	AStrikeCharacter* Character = Cast<AStrikeCharacter>(OtherActor);
	if (!bActive || !Character || Character->IsDead() || PickupType == EStrikePickupType::Weapon)
	{
		return;
	}
	// Seuls les joueurs et bots (avec PlayerState) ramassent ; pas les zombies ni ennemis d'extraction.
	if (!Character->GetPlayerState())
	{
		return;
	}
	if (GiveTo(Character))
	{
		SetActive(false);
	}
}

bool AStrikePickup::GiveTo(AStrikeCharacter* User)
{
	switch (PickupType)
	{
	case EStrikePickupType::Weapon:
		return User->GetWeaponComponent()->GiveWeapon(WeaponClass, true) != nullptr;

	case EStrikePickupType::Ammo:
		return User->GetInventoryComponent()->AddAmmo(AmmoType, Amount) > 0;

	case EStrikePickupType::Armor:
	{
		UHealthComponent* Health = User->GetHealthComponent();
		if (Health->GetArmor() >= Health->MaxArmor)
		{
			return false;
		}
		Health->AddArmor(Amount);
		return true;
	}

	case EStrikePickupType::Health:
	{
		UHealthComponent* Health = User->GetHealthComponent();
		if (Health->GetHealth() >= Health->MaxHealth)
		{
			return false;
		}
		Health->Heal(Amount);
		return true;
	}

	case EStrikePickupType::Loot:
		User->GetInventoryComponent()->AddLoot(LootItem);
		return true;
	}
	return false;
}

void AStrikePickup::SetActive(bool bNewActive)
{
	bActive = bNewActive;
	OnRep_Active();
	if (!bActive && HasAuthority())
	{
		if (RespawnTime > 0.f)
		{
			GetWorldTimerManager().SetTimer(RespawnHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { SetActive(true); }), RespawnTime, false);
		}
		else
		{
			SetLifeSpan(1.f);
		}
	}
}

void AStrikePickup::OnRep_Active()
{
	Mesh->SetVisibility(bActive);
	Trigger->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	Mesh->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

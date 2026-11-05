// STRIKE ZONE - équipement et gestion des armes d'un personnage
#include "Components/WeaponComponent.h"
#include "Components/InventoryComponent.h"
#include "Character/StrikeCharacter.h"
#include "Weapons/StrikeWeapon.h"

#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UWeaponComponent::UWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UWeaponComponent, Weapons);
	DOREPLIFETIME(UWeaponComponent, CurrentWeapon);
}

AStrikeCharacter* UWeaponComponent::GetCharacter() const
{
	return Cast<AStrikeCharacter>(GetOwner());
}

AStrikeWeapon* UWeaponComponent::FindWeaponOfClass(TSubclassOf<AStrikeWeapon> WeaponClass) const
{
	for (AStrikeWeapon* Weapon : Weapons)
	{
		if (Weapon && Weapon->GetClass() == WeaponClass)
		{
			return Weapon;
		}
	}
	return nullptr;
}

int32 UWeaponComponent::GetCurrentIndex() const
{
	return Weapons.IndexOfByKey(CurrentWeapon);
}

AStrikeWeapon* UWeaponComponent::GiveWeapon(TSubclassOf<AStrikeWeapon> WeaponClass, bool bEquip)
{
	AStrikeCharacter* Character = GetCharacter();
	if (!Character || !Character->HasAuthority() || !WeaponClass)
	{
		return nullptr;
	}

	UInventoryComponent* Inventory = Character->GetInventoryComponent();
	const AStrikeWeapon* WeaponCDO = WeaponClass->GetDefaultObject<AStrikeWeapon>();

	// Déjà possédée : munitions.
	if (AStrikeWeapon* Existing = FindWeaponOfClass(WeaponClass))
	{
		Existing->ServerSetMagazineFull();
		if (Inventory)
		{
			Inventory->AddAmmo(WeaponCDO->AmmoType, WeaponCDO->MagazineSize * WeaponCDO->StartingReserveMagazines);
		}
		return Existing;
	}

	// Inventaire plein : remplace l'arme active.
	if (Weapons.Num() >= MaxWeapons && CurrentWeapon)
	{
		AStrikeWeapon* Replaced = CurrentWeapon;
		Weapons.Remove(Replaced);
		SetCurrentWeapon(nullptr, Replaced);
		Replaced->Destroy();
	}

	FActorSpawnParameters Params;
	Params.Owner = Character;
	Params.Instigator = Character;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AStrikeWeapon* NewWeapon = GetWorld()->SpawnActor<AStrikeWeapon>(WeaponClass, Character->GetActorTransform(), Params);
	if (!NewWeapon)
	{
		return nullptr;
	}
	NewWeapon->SetOwningCharacter(Character);
	Weapons.Add(NewWeapon);

	if (Inventory)
	{
		Inventory->AddAmmo(NewWeapon->AmmoType, NewWeapon->MagazineSize * NewWeapon->StartingReserveMagazines);
	}

	if (bEquip || !CurrentWeapon)
	{
		SetCurrentWeapon(NewWeapon, CurrentWeapon);
	}
	else
	{
		NewWeapon->OnUnequipped();
	}
	return NewWeapon;
}

void UWeaponComponent::DestroyAllWeapons()
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	for (AStrikeWeapon* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->Destroy();
		}
	}
	Weapons.Reset();
	CurrentWeapon = nullptr;
	OnWeaponChanged.Broadcast(nullptr);
}

void UWeaponComponent::EquipWeaponAtIndex(int32 Index)
{
	if (!Weapons.IsValidIndex(Index) || Weapons[Index] == CurrentWeapon)
	{
		return;
	}

	// Prédiction locale immédiate.
	SetCurrentWeapon(Weapons[Index], CurrentWeapon);

	if (!GetOwner()->HasAuthority())
	{
		ServerEquipWeaponAtIndex(Index);
	}
}

void UWeaponComponent::ServerEquipWeaponAtIndex_Implementation(int32 Index)
{
	if (Weapons.IsValidIndex(Index) && Weapons[Index] != CurrentWeapon)
	{
		SetCurrentWeapon(Weapons[Index], CurrentWeapon);
	}
}

void UWeaponComponent::CycleWeapon(int32 Direction)
{
	if (Weapons.Num() < 2)
	{
		return;
	}
	const int32 Current = FMath::Max(0, GetCurrentIndex());
	const int32 Next = (Current + (Direction >= 0 ? 1 : -1) + Weapons.Num()) % Weapons.Num();
	EquipWeaponAtIndex(Next);
}

void UWeaponComponent::SetCurrentWeapon(AStrikeWeapon* NewWeapon, AStrikeWeapon* OldWeapon)
{
	if (OldWeapon && OldWeapon != NewWeapon)
	{
		OldWeapon->OnUnequipped();
	}
	CurrentWeapon = NewWeapon;
	if (NewWeapon)
	{
		NewWeapon->OnEquipped();
	}
	OnWeaponChanged.Broadcast(NewWeapon);
}

void UWeaponComponent::OnRep_CurrentWeapon()
{
	// Le propriétaire a pu prédire le changement : on synchronise simplement l'état visuel.
	for (AStrikeWeapon* Weapon : Weapons)
	{
		if (Weapon && Weapon != CurrentWeapon && Weapon->IsEquipped())
		{
			Weapon->OnUnequipped();
		}
	}
	if (CurrentWeapon && !CurrentWeapon->IsEquipped())
	{
		CurrentWeapon->OnEquipped();
	}
	OnWeaponChanged.Broadcast(CurrentWeapon);
}

void UWeaponComponent::OnRep_Weapons()
{
	OnWeaponChanged.Broadcast(CurrentWeapon);
}

void UWeaponComponent::StartFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartFire();
	}
}

void UWeaponComponent::StopFire()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StopFire();
	}
}

void UWeaponComponent::StartReload()
{
	if (CurrentWeapon)
	{
		CurrentWeapon->StartReload();
	}
}

// STRIKE ZONE - réserves de munitions et butin transporté
#include "Components/InventoryComponent.h"
#include "Net/UnrealNetwork.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	// Pistol, Rifle, Shotgun, Sniper, Rocket
	MaxAmmoPerType = { 150, 300, 48, 40, 6 };
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UInventoryComponent, Ammo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UInventoryComponent, Loot, COND_OwnerOnly);
}

FStrikeAmmoStack* UInventoryComponent::FindStack(EStrikeAmmoType Type)
{
	return Ammo.FindByPredicate([Type](const FStrikeAmmoStack& S) { return S.AmmoType == Type; });
}

const FStrikeAmmoStack* UInventoryComponent::FindStack(EStrikeAmmoType Type) const
{
	return Ammo.FindByPredicate([Type](const FStrikeAmmoStack& S) { return S.AmmoType == Type; });
}

int32 UInventoryComponent::GetMaxAmmo(EStrikeAmmoType Type) const
{
	const int32 Index = static_cast<int32>(Type);
	return MaxAmmoPerType.IsValidIndex(Index) ? MaxAmmoPerType[Index] : 999;
}

int32 UInventoryComponent::GetAmmo(EStrikeAmmoType Type) const
{
	if (bInfiniteAmmo)
	{
		return GetMaxAmmo(Type);
	}
	const FStrikeAmmoStack* Stack = FindStack(Type);
	return Stack ? Stack->Count : 0;
}

int32 UInventoryComponent::AddAmmo(EStrikeAmmoType Type, int32 Amount)
{
	if (!GetOwner()->HasAuthority() || Amount <= 0)
	{
		return 0;
	}
	FStrikeAmmoStack* Stack = FindStack(Type);
	if (!Stack)
	{
		FStrikeAmmoStack NewStack;
		NewStack.AmmoType = Type;
		NewStack.Count = 0;
		Stack = &Ammo.Add_GetRef(NewStack);
	}
	const int32 Before = Stack->Count;
	Stack->Count = FMath::Min(GetMaxAmmo(Type), Stack->Count + Amount);
	OnInventoryChanged.Broadcast();
	return Stack->Count - Before;
}

int32 UInventoryComponent::ConsumeAmmo(EStrikeAmmoType Type, int32 Requested)
{
	if (!GetOwner()->HasAuthority() || Requested <= 0)
	{
		return 0;
	}
	if (bInfiniteAmmo)
	{
		return Requested;
	}
	FStrikeAmmoStack* Stack = FindStack(Type);
	if (!Stack)
	{
		return 0;
	}
	const int32 Taken = FMath::Min(Requested, Stack->Count);
	Stack->Count -= Taken;
	OnInventoryChanged.Broadcast();
	return Taken;
}

void UInventoryComponent::FillAllAmmo()
{
	for (int32 i = 0; i < static_cast<int32>(EStrikeAmmoType::MAX); ++i)
	{
		const EStrikeAmmoType Type = static_cast<EStrikeAmmoType>(i);
		AddAmmo(Type, GetMaxAmmo(Type));
	}
}

void UInventoryComponent::AddLoot(const FStrikeLootItem& Item)
{
	if (GetOwner()->HasAuthority())
	{
		Loot.Add(Item);
		OnInventoryChanged.Broadcast();
	}
}

void UInventoryComponent::ClearLoot()
{
	if (GetOwner()->HasAuthority())
	{
		Loot.Reset();
		OnInventoryChanged.Broadcast();
	}
}

int32 UInventoryComponent::GetLootValue() const
{
	int32 Total = 0;
	for (const FStrikeLootItem& Item : Loot)
	{
		Total += Item.Value;
	}
	return Total;
}

void UInventoryComponent::OnRep_Inventory()
{
	OnInventoryChanged.Broadcast();
}

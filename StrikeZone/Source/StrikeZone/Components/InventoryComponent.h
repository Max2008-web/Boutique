// STRIKE ZONE - réserves de munitions et butin transporté
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/StrikeTypes.h"
#include "InventoryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStrikeOnInventoryChanged);

/**
 * Inventaire serveur-autoritaire : munitions de réserve par calibre et objets de butin.
 * Répliqué uniquement au propriétaire (inutile pour les autres clients).
 */
UCLASS(ClassGroup = (Strike), meta = (BlueprintSpawnableComponent))
class STRIKEZONE_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Munitions infinies (Entraînement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	bool bInfiniteAmmo = false;

	UPROPERTY(BlueprintAssignable, Category = "Inventory")
	FStrikeOnInventoryChanged OnInventoryChanged;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetAmmo(EStrikeAmmoType Type) const;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetMaxAmmo(EStrikeAmmoType Type) const;

	/** Serveur : ajoute des munitions (plafonnées). Retourne la quantité ajoutée. */
	int32 AddAmmo(EStrikeAmmoType Type, int32 Amount);

	/** Serveur : retire jusqu'à Requested munitions. Retourne la quantité obtenue. */
	int32 ConsumeAmmo(EStrikeAmmoType Type, int32 Requested);

	/** Serveur : remplit toutes les réserves. */
	void FillAllAmmo();

	/** Serveur : butin. */
	void AddLoot(const FStrikeLootItem& Item);
	void ClearLoot();
	const TArray<FStrikeLootItem>& GetLoot() const { return Loot; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetLootValue() const;

protected:
	UFUNCTION()
	void OnRep_Inventory();

	FStrikeAmmoStack* FindStack(EStrikeAmmoType Type);
	const FStrikeAmmoStack* FindStack(EStrikeAmmoType Type) const;

	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	TArray<FStrikeAmmoStack> Ammo;

	UPROPERTY(ReplicatedUsing = OnRep_Inventory)
	TArray<FStrikeLootItem> Loot;

	/** Capacités maximales par calibre (index = EStrikeAmmoType). */
	UPROPERTY(EditAnywhere, Category = "Inventory")
	TArray<int32> MaxAmmoPerType;
};

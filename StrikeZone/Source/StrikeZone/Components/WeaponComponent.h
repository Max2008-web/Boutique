// STRIKE ZONE - équipement et gestion des armes d'un personnage
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponComponent.generated.h"

class AStrikeWeapon;
class AStrikeCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStrikeOnWeaponChanged, AStrikeWeapon*, NewWeapon);

/**
 * Possède les armes (acteurs répliqués), gère l'arme active et relaie tir/rechargement.
 * Le changement d'arme est prédit localement puis confirmé par le serveur.
 */
UCLASS(ClassGroup = (Strike), meta = (BlueprintSpawnableComponent))
class STRIKEZONE_API UWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapons")
	int32 MaxWeapons = 3;

	UPROPERTY(BlueprintAssignable, Category = "Weapons")
	FStrikeOnWeaponChanged OnWeaponChanged;

	/**
	 * Serveur : donne une arme. Si elle est déjà possédée, recharge ses munitions.
	 * Si l'inventaire est plein, remplace l'arme active.
	 */
	AStrikeWeapon* GiveWeapon(TSubclassOf<AStrikeWeapon> WeaponClass, bool bEquip = true);

	/** Serveur : détruit toutes les armes. */
	void DestroyAllWeapons();

	void EquipWeaponAtIndex(int32 Index);
	void CycleWeapon(int32 Direction);
	void StartFire();
	void StopFire();
	void StartReload();

	UFUNCTION(BlueprintPure, Category = "Weapons")
	AStrikeWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	const TArray<TObjectPtr<AStrikeWeapon>>& GetWeapons() const { return Weapons; }
	AStrikeWeapon* FindWeaponOfClass(TSubclassOf<AStrikeWeapon> WeaponClass) const;
	int32 GetCurrentIndex() const;

protected:
	UFUNCTION()
	void OnRep_CurrentWeapon();

	UFUNCTION()
	void OnRep_Weapons();

	UFUNCTION(Server, Reliable)
	void ServerEquipWeaponAtIndex(int32 Index);

	void SetCurrentWeapon(AStrikeWeapon* NewWeapon, AStrikeWeapon* OldWeapon);
	AStrikeCharacter* GetCharacter() const;

	UPROPERTY(ReplicatedUsing = OnRep_Weapons)
	TArray<TObjectPtr<AStrikeWeapon>> Weapons;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon)
	TObjectPtr<AStrikeWeapon> CurrentWeapon;
};

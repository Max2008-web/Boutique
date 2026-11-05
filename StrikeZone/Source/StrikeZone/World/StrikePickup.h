// STRIKE ZONE - objets ramassables (armes, munitions, armure, soins, butin)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/StrikeTypes.h"
#include "StrikePickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AStrikeWeapon;

UENUM(BlueprintType)
enum class EStrikePickupType : uint8
{
	Weapon,
	Ammo,
	Armor,
	Health,
	Loot
};

/**
 * Ramassage serveur-autoritaire. Munitions/armure/soins/butin : automatique au contact.
 * Armes : touche d'interaction (choix volontaire du joueur).
 */
UCLASS()
class STRIKEZONE_API AStrikePickup : public AActor, public IStrikeInteractable
{
	GENERATED_BODY()

public:
	AStrikePickup();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// --- Configuration (serveur, juste après l'apparition) ---
	void SetupWeapon(TSubclassOf<AStrikeWeapon> InWeaponClass);
	void SetupAmmo(EStrikeAmmoType InAmmoType, int32 InAmount);
	void SetupArmor(int32 InAmount);
	void SetupHealth(int32 InAmount);
	void SetupLoot(const FStrikeLootItem& InItem);

	/** Réapparition après ramassage (0 = jamais). */
	UPROPERTY(EditAnywhere, Category = "Pickup")
	float RespawnTime = 0.f;

	// --- IStrikeInteractable ---
	virtual bool CanInteract(const AStrikeCharacter* User) const override;
	virtual void Interact(AStrikeCharacter* User) override;
	virtual FString GetInteractPrompt(const AStrikeCharacter* User) const override;

protected:
	UFUNCTION()
	void OnTriggerOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnRep_Active();

	UFUNCTION()
	void OnRep_Config();

	bool GiveTo(AStrikeCharacter* User);
	void SetActive(bool bNewActive);
	void RefreshVisual();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USphereComponent> Trigger;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeshMaterial;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Config, Category = "Pickup")
	EStrikePickupType PickupType = EStrikePickupType::Ammo;

	UPROPERTY(EditAnywhere, ReplicatedUsing = OnRep_Config, Category = "Pickup")
	TSubclassOf<AStrikeWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, Replicated, Category = "Pickup")
	EStrikeAmmoType AmmoType = EStrikeAmmoType::Rifle;

	UPROPERTY(EditAnywhere, Replicated, Category = "Pickup")
	int32 Amount = 30;

	UPROPERTY(EditAnywhere, Replicated, Category = "Pickup")
	FStrikeLootItem LootItem;

	UPROPERTY(ReplicatedUsing = OnRep_Active)
	bool bActive = true;

	FTimerHandle RespawnHandle;
	float BaseZ = 0.f;
};

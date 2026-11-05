// STRIKE ZONE - personnage FPS jouable (joueurs, bots, base des zombies)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/StrikeTypes.h"
#include "StrikeCharacter.generated.h"

class UCameraComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class UHealthComponent;
class UWeaponComponent;
class UInventoryComponent;
class UStrikeCharacterMovement;
class AStrikeWeapon;

/**
 * Personnage FPS.
 * - Caméra première personne avec transition ADS (FOV + position d'arme).
 * - Sprint / glissade prédits (UStrikeCharacterMovement), saut, accroupissement.
 * - Santé/armure, armes et inventaire via composants réutilisables.
 * - Corps temporaire (primitives moteur) tant qu'aucun Skeletal Mesh n'est fourni.
 */
UCLASS(Config = Game)
class STRIKEZONE_API AStrikeCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AStrikeCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
	virtual bool CanJumpInternal_Implementation() const override;
	virtual void Destroyed() override;

	// ----- API d'entrée (appelée par AStrikePlayerController ou par l'IA) -----
	void MoveInput(const FVector2D& Axis);
	void StartJump();
	void StopJump();
	void StartCrouchOrSlide();
	void StopCrouchOrSlide();
	void ToggleCrouch();
	void SetSprinting(bool bNewSprinting);
	void StartFire();
	void StopFire();
	void SetAiming(bool bNewAiming);
	void ReloadWeapon();
	void Interact();
	void SetInteractHeld(bool bHeld);
	void EquipSlot(int32 SlotIndex);
	void CycleWeapon(int32 Direction);

	// ----- État -----
	UFUNCTION(BlueprintPure, Category = "Strike") bool IsAiming() const { return bIsAiming; }
	UFUNCTION(BlueprintPure, Category = "Strike") float GetAimAlpha() const { return AimAlpha; }
	UFUNCTION(BlueprintPure, Category = "Strike") bool IsSprinting() const;
	UFUNCTION(BlueprintPure, Category = "Strike") bool IsSliding() const;
	UFUNCTION(BlueprintPure, Category = "Strike") bool IsDead() const;
	UFUNCTION(BlueprintPure, Category = "Strike") uint8 GetTeamId() const { return TeamId; }
	bool IsInteractHeld() const { return bInteractHeld; }

	/** Fixé par le serveur (GameMode) ; répliqué. */
	void SetTeamId(uint8 NewTeam);

	/** Cherche l'objet interactif visé ou le plus proche (client : affichage, serveur : validation). */
	AActor* FindInteractable() const;

	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
	USceneComponent* GetWeaponAttachPoint() const { return WeaponAttachPoint; }
	UHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UWeaponComponent* GetWeaponComponent() const { return WeaponComponent; }
	UInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }
	UStrikeCharacterMovement* GetStrikeMovement() const;
	AStrikeWeapon* GetCurrentWeapon() const;

	/** FOV de base (réglage joueur). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Strike|Camera")
	float BaseFieldOfView = 90.f;

	/** Distance maximale d'interaction. */
	UPROPERTY(EditDefaultsOnly, Category = "Strike|Interaction")
	float InteractRange = 260.f;

protected:
	UFUNCTION(Server, Reliable)
	void ServerSetAiming(bool bNewAiming);

	UFUNCTION(Server, Reliable)
	void ServerInteract();

	UFUNCTION(Server, Reliable)
	void ServerSetInteractHeld(bool bHeld);

	UFUNCTION()
	void HandleDeath(UHealthComponent* HealthComp, AController* Killer, AActor* DamageCauser, bool bHeadshot);

	UFUNCTION()
	void OnRep_TeamId();

	/** Effets cosmétiques de mort (tous les postes). */
	virtual void PlayDeathCosmetics();

	void UpdateCamera(float DeltaSeconds);
	void ApplyTeamColor();

	// ----- Composants -----
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	/** Point d'attache de l'arme (enfant de la caméra : suit la visée). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> WeaponAttachPoint;

	/** Corps temporaire visible par les autres joueurs. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWeaponComponent> WeaponComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UInventoryComponent> InventoryComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	// ----- État répliqué -----
	UPROPERTY(Replicated)
	bool bIsAiming = false;

	UPROPERTY(ReplicatedUsing = OnRep_TeamId)
	uint8 TeamId = StrikeTeams::NoTeam;

	/** Serveur uniquement : touche d'interaction maintenue (pose/désamorçage, extraction). */
	bool bInteractHeld = false;

	bool bCrouchToggled = false;
	bool bDeathCosmeticsPlayed = false;
	float AimAlpha = 0.f;
	float CurrentEyeZ = 0.f;
};

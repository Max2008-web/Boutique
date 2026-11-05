// STRIKE ZONE - classe de base des armes (hitscan et projectiles, tir validé par le serveur)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/StrikeTypes.h"
#include "StrikeWeapon.generated.h"

class AStrikeCharacter;
class AStrikeProjectile;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class USoundBase;
class UAnimMontage;

/**
 * Arme de base.
 *
 * Flux réseau d'un tir :
 *  1. Le client local prédit : cadence, munitions, recul, effets (réactivité immédiate).
 *  2. Il envoie ServerFireShot(origine, direction, graine).
 *  3. Le serveur valide (cadence par jetons, munitions, origine proche de l'œil serveur,
 *     angle cohérent avec la rotation de contrôle), applique la dispersion avec la graine
 *     et effectue lui-même les traces + dégâts. Le client ne déclare jamais de touche.
 *  4. Le serveur confirme les touches (hit marker) et diffuse les effets aux autres clients.
 */
UCLASS(Abstract, Blueprintable)
class STRIKEZONE_API AStrikeWeapon : public AActor
{
	GENERATED_BODY()

public:
	AStrikeWeapon();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ======================= Caractéristiques =======================
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FString WeaponName = TEXT("Arme");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EStrikeFireType FireType = EStrikeFireType::Hitscan;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	EStrikeAmmoType AmmoType = EStrikeAmmoType::Rifle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 MagazineSize = 30;

	/** Chargeurs de réserve donnés avec l'arme. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	int32 StartingReserveMagazines = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	float Damage = 25.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	FStrikeZoneMultipliers ZoneMultipliers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	float MaxRange = 15000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	float FalloffStart = 2500.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	float FalloffEnd = 6000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	float MinDamageFactor = 0.6f;

	/** Coups par minute. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	float FireRateRPM = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	bool bAutomatic = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	int32 PelletsPerShot = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	TSubclassOf<AStrikeProjectile> ProjectileClass;

	/** Portée du bruit de tir pour la perception auditive de l'IA. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	float NoiseRange = 4000.f;

	// --- Dispersion (en degrés, demi-angle du cône) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float HipSpread = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float AimSpread = 0.3f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float SpreadPerShot = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float MaxSpread = 6.f;

	/** Degrés récupérés par seconde. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float SpreadRecovery = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float MovingSpreadMultiplier = 1.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float AirSpreadMultiplier = 3.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Spread")
	float CrouchSpreadMultiplier = 0.7f;

	// --- Recul (degrés) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilPitchMin = 0.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilPitchMax = 0.8f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilYaw = 0.25f;

	/** Vitesse de retour de la visée après une rafale (0 = aucun retour). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Recoil")
	float RecoilRecoverySpeed = 6.f;

	// --- Visée / rechargement ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim")
	float AimFOV = 65.f;

	/** Vitesse de transition ADS (1/secondes). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim")
	float AimSpeed = 6.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Reload")
	float ReloadTime = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Reload")
	float EquipTime = 0.4f;

	/** Prix d'achat (mode Zombies). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Economy")
	int32 BuyPrice = 1000;

	// --- Présentation (temporaire tant qu'aucun mesh n'est fourni) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	FLinearColor PlaceholderColor = FLinearColor(0.08f, 0.08f, 0.08f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	FVector PlaceholderScale = FVector(0.6f, 0.07f, 0.1f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	FVector HipOffset = FVector(38.f, 16.f, -17.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Visual")
	FVector AimOffset = FVector(30.f, 0.f, -8.f);

	// --- Effets optionnels (assets à fournir ; aucun n'est requis) ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> MuzzleFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	TObjectPtr<UNiagaraSystem> ImpactFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	TObjectPtr<UAnimMontage> FireMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	TObjectPtr<UAnimMontage> ReloadMontage;

	/** Traceur visible (fonctionne sans asset). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	bool bSpawnTracers = true;

	// ======================= API =======================
	/** Serveur : rattache l'arme à son porteur. */
	void SetOwningCharacter(AStrikeCharacter* NewOwner);
	AStrikeCharacter* GetOwningCharacter() const { return OwningCharacter; }

	void OnEquipped();
	void OnUnequipped();
	bool IsEquipped() const { return bIsEquipped; }

	void StartFire();
	void StopFire();
	void StartReload();

	bool CanFire() const;
	bool CanReload() const;

	/** Munitions du chargeur (valeur prédite pour le propriétaire local). */
	UFUNCTION(BlueprintPure, Category = "Weapon") int32 GetCurrentAmmo() const;
	UFUNCTION(BlueprintPure, Category = "Weapon") int32 GetReserveAmmo() const;
	UFUNCTION(BlueprintPure, Category = "Weapon") bool IsReloading() const { return bIsReloading; }
	UFUNCTION(BlueprintPure, Category = "Weapon") float GetCurrentSpread() const;

	float GetFireInterval() const { return 60.f / FMath::Max(FireRateRPM, 1.f); }

	/** Serveur : remplit le chargeur (achats, pickups). */
	void ServerSetMagazineFull();

protected:
	// --- Logique de tir ---
	void HandleFireLoop();
	void FireShotLocal();
	void ProcessShotAuthority(const FVector& Origin, const FVector& AimDirection, int32 Seed);
	void ConsumeSpreadForShot();
	FVector ApplySpread(const FVector& Direction, FRandomStream& Stream, float SpreadDegrees) const;
	float GetDamageFalloff(float Distance) const;
	bool IsLocallyControlledOwner() const;
	void GetAimPoint(FVector& OutOrigin, FVector& OutDirection) const;

	// --- Cosmétique ---
	void PlayFireCosmetics(const FVector& TraceEnd);
	void PlayImpactCosmetics(const FVector& Location, const FVector& Normal);
	void UpdateWeaponPose(float DeltaSeconds);
	void ApplyRecoilToController(float DeltaSeconds);
	void RefreshAttachment();

	void FinishReload();

	// --- RPC ---
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFireShot(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, int32 Seed);

	UFUNCTION(Server, Reliable)
	void ServerStartReload();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastFireFX(FVector_NetQuantize TraceEnd, FVector_NetQuantizeNormal ImpactNormal, bool bHit);

	UFUNCTION(Client, Unreliable)
	void ClientConfirmHit(bool bHeadshot, bool bKill);

	UFUNCTION()
	void OnRep_OwningCharacter();

	UFUNCTION()
	void OnRep_Equipped();

	UFUNCTION()
	void OnRep_Reloading();

	// --- Composants ---
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Muzzle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> MuzzleLight;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> MeshMaterial;

	// --- État répliqué ---
	UPROPERTY(ReplicatedUsing = OnRep_OwningCharacter)
	TObjectPtr<AStrikeCharacter> OwningCharacter;

	/** Munitions du chargeur (autorité serveur). */
	UPROPERTY(Replicated)
	int32 CurrentAmmo = 0;

	/**
	 * Nombre de demandes de tir traitées par le serveur (propriétaire uniquement).
	 * Munitions prédites = CurrentAmmo - (LocalShotCount - ServerShotCount).
	 */
	UPROPERTY(Replicated)
	int32 ServerShotCount = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Reloading)
	bool bIsReloading = false;

	UPROPERTY(ReplicatedUsing = OnRep_Equipped)
	bool bIsEquipped = false;

	// --- État local ---
	bool bWantsToFire = false;
	bool bFiredSinceTriggerPull = false;
	float LastLocalFireTime = -100.f;
	float EquipReadyTime = 0.f;
	float SpreadBonus = 0.f;
	int32 LocalShotCount = 0;
	FRotator PendingRecoil = FRotator::ZeroRotator;
	float AccumulatedRecoilPitch = 0.f;
	float MuzzleFlashRemaining = 0.f;
	float KickBack = 0.f;
	FTimerHandle FireLoopTimer;
	FTimerHandle ReloadTimer;

	// --- État serveur (anti-triche) ---
	float ServerFireCredits = 2.f;
	float ServerLastCreditTime = 0.f;
};

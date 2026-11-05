// STRIKE ZONE - achats muraux (Zombies), zones d'objectif (Tactique/Extraction), cibles (Entraînement), zone BR
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/StrikeTypes.h"
#include "StrikeInteractables.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;
class AStrikeWeapon;
class AStrikeCharacter;

// ===========================================================================
// Achat mural
// ===========================================================================

/** Achat d'arme avec la monnaie de partie (mode Zombies). Rachat = munitions à moitié prix. */
UCLASS()
class STRIKEZONE_API AStrikeWeaponBuy : public AActor, public IStrikeInteractable
{
	GENERATED_BODY()

public:
	AStrikeWeaponBuy();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Serveur. */
	void Setup(TSubclassOf<AStrikeWeapon> InWeaponClass, int32 InPrice);

	virtual bool CanInteract(const AStrikeCharacter* User) const override;
	virtual void Interact(AStrikeCharacter* User) override;
	virtual FString GetInteractPrompt(const AStrikeCharacter* User) const override;

protected:
	int32 GetPriceFor(const AStrikeCharacter* User) const;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Panel;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WeaponPreview;

	UPROPERTY(EditAnywhere, Replicated, Category = "Buy")
	TSubclassOf<AStrikeWeapon> WeaponClass;

	UPROPERTY(EditAnywhere, Replicated, Category = "Buy")
	int32 Price = 1000;
};

// ===========================================================================
// Zone d'objectif
// ===========================================================================

UENUM(BlueprintType)
enum class EStrikeObjectiveType : uint8
{
	ChargeSite,
	Extraction
};

/** Zone circulaire au sol ; la logique (pose, désamorçage, extraction) est dans le GameMode. */
UCLASS()
class STRIKEZONE_API AStrikeObjectiveZone : public AActor
{
	GENERATED_BODY()

public:
	AStrikeObjectiveZone();
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Serveur : personnages vivants dans la zone. */
	TArray<AStrikeCharacter*> GetCharactersInside() const;
	bool IsInside(const AActor* Actor) const;

	void SetZoneActive(bool bNewActive);
	bool IsZoneActive() const { return bZoneActive; }

	UPROPERTY(EditAnywhere, Replicated, Category = "Objective")
	EStrikeObjectiveType ObjectiveType = EStrikeObjectiveType::ChargeSite;

	UPROPERTY(EditAnywhere, Replicated, Category = "Objective")
	FString Label = TEXT("A");

	UPROPERTY(EditAnywhere, Replicated, Category = "Objective")
	float Radius = 350.f;

	/** Serveur : charge posée sur ce site (Tactique). */
	UPROPERTY(Replicated)
	bool bChargePlanted = false;

	void RefreshVisual();

protected:
	UFUNCTION()
	void OnRep_Visual();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Disk;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Beacon;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DiskMaterial;

	UPROPERTY(ReplicatedUsing = OnRep_Visual)
	bool bZoneActive = true;
};

// ===========================================================================
// Cible d'entraînement
// ===========================================================================

/** Cible de stand de tir : tête marquée "Head", tombe quand elle est "éliminée", option mobile. */
UCLASS()
class STRIKEZONE_API AStrikeTarget : public AActor
{
	GENERATED_BODY()

public:
	AStrikeTarget();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(EditAnywhere, Category = "Target")
	float MaxHealth = 100.f;

	/** Amplitude du déplacement latéral (0 = fixe). */
	UPROPERTY(EditAnywhere, Replicated, Category = "Target")
	float MoveAmplitude = 0.f;

	UPROPERTY(EditAnywhere, Replicated, Category = "Target")
	float MoveSpeed = 1.f;

	UPROPERTY(EditAnywhere, Category = "Target")
	float ResetDelay = 2.f;

protected:
	UFUNCTION()
	void OnRep_Down();

	void ResetTarget();

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<USceneComponent> Pivot;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Stand;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Head;

	UPROPERTY(ReplicatedUsing = OnRep_Down)
	bool bDown = false;

	float Health = 100.f;
	FVector Origin = FVector::ZeroVector;
	FTimerHandle ResetHandle;
};

// ===========================================================================
// Zone du Battle Royale (visuel)
// ===========================================================================

/** Mur de zone en fil de fer, piloté par AStrikeGameState (centre / rayon répliqués). */
UCLASS()
class STRIKEZONE_API AStrikeStormZone : public AActor
{
	GENERATED_BODY()

public:
	AStrikeStormZone();
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Wall;

	float DisplayedRadius = 0.f;
};

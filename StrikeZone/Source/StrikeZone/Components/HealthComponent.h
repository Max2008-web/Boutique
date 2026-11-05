// STRIKE ZONE - santé, armure, dégâts localisés (autorité serveur)
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/StrikeTypes.h"
#include "HealthComponent.generated.h"

class UHealthComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FStrikeOnHealthChanged, UHealthComponent*, HealthComp, float, Health, float, Armor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FStrikeOnDeath, UHealthComponent*, HealthComp, AController*, Killer, AActor*, DamageCauser, bool, bHeadshot);

/**
 * Santé + armure. Toutes les modifications passent par le serveur ; les clients reçoivent
 * les valeurs répliquées et une notification de direction des dégâts pour le HUD.
 */
UCLASS(ClassGroup = (Strike), meta = (BlueprintSpawnableComponent))
class STRIKEZONE_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float MaxArmor = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health")
	float StartingArmor = 0.f;

	/** Part des dégâts absorbée par l'armure tant qu'elle n'est pas vide. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health", meta = (ClampMin = "0", ClampMax = "1"))
	float ArmorAbsorption = 0.6f;

	/** Régénération façon FPS moderne (désactivée pour Tactique / Battle Royale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regen")
	bool bRegenerate = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regen")
	float RegenDelay = 5.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Health|Regen")
	float RegenPerSecond = 25.f;

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FStrikeOnHealthChanged OnHealthChanged;

	/** Diffusé sur le serveur (avec le tueur) puis sur les clients via réplication (tueur nul). */
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FStrikeOnDeath OnDeath;

	/** Serveur : applique des dégâts (déjà multipliés par la zone). Retourne les dégâts réellement subis. */
	float ApplyDamage(float Amount, AController* InstigatorController, AActor* DamageCauser, EStrikeHitZone Zone, const FVector& SourceLocation);

	void Heal(float Amount);
	void AddArmor(float Amount);
	void Kill(AController* Killer);
	void ResetHealth();
	void SetMaxHealth(float NewMax, bool bFill);

	UFUNCTION(BlueprintPure, Category = "Health") float GetHealth() const { return Health; }
	UFUNCTION(BlueprintPure, Category = "Health") float GetArmor() const { return Armor; }
	UFUNCTION(BlueprintPure, Category = "Health") float GetHealthFraction() const { return MaxHealth > 0.f ? Health / MaxHealth : 0.f; }
	UFUNCTION(BlueprintPure, Category = "Health") bool IsDead() const { return bIsDead; }

	/** Détermine la zone touchée : par os (Skeletal Mesh) ou par hauteur sur la capsule. */
	static EStrikeHitZone GetHitZoneFromHit(const FHitResult& Hit, const AActor* Victim);

	/** Client local : dernière source de dégâts pour l'indicateur directionnel du HUD. */
	FVector LastDamageSource = FVector::ZeroVector;
	float LastDamageClientTime = -100.f;

protected:
	UFUNCTION()
	void OnRep_Health();

	UFUNCTION()
	void OnRep_IsDead();

	UFUNCTION(Client, Unreliable)
	void ClientNotifyDamaged(FVector_NetQuantize SourceLocation);

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Armor = 0.f;

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	float LastDamageTime = -100.f;
};

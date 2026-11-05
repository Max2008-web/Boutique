// STRIKE ZONE - projectiles physiques (roquettes, grenades)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrikeProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class UPointLightComponent;
class UNiagaraSystem;
class USoundBase;

/**
 * Projectile répliqué. La collision et les dégâts sont résolus exclusivement sur le serveur ;
 * les clients voient le mouvement répliqué et l'explosion via bExploded (RepNotify).
 */
UCLASS(Blueprintable)
class STRIKEZONE_API AStrikeProjectile : public AActor
{
	GENERATED_BODY()

public:
	AStrikeProjectile();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Serveur : appelé juste après l'apparition. */
	void InitProjectile(float InDirectDamage, const FString& InWeaponName);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float DirectDamage = 50.f;

	/** 0 = pas d'explosion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float ExplosionRadius = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float ExplosionDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	float ExplosionImpulse = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|FX")
	TObjectPtr<UNiagaraSystem> ExplosionFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|FX")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|FX")
	FLinearColor PlaceholderColor = FLinearColor(1.f, 0.5f, 0.1f);

	FString GetWeaponName() const { return WeaponName; }

protected:
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	UFUNCTION()
	void OnRep_Exploded();

	void PlayExplosionCosmetics();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Visual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UPointLightComponent> Glow;

	UPROPERTY(ReplicatedUsing = OnRep_Exploded)
	bool bExploded = false;

	UPROPERTY(Replicated)
	FVector_NetQuantize ExplosionLocation;

	FString WeaponName;
};

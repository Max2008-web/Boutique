// STRIKE ZONE - base C++ des Animation Blueprints (variables pour State Machines, IK, montages)
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "StrikeAnimInstance.generated.h"

class AStrikeCharacter;

/**
 * Parent des Animation Blueprints du personnage. Expose l'état de jeu calculé en C++
 * (thread-safe via NativeUpdateAnimation) pour alimenter une State Machine :
 * Idle/Walk/Sprint (Speed, Direction), Crouch, Slide, Jump/Fall (bIsInAir),
 * Aim Offset (AimPitch/AimYaw), ADS (AimAlpha), rechargement, mort.
 * Les montages de tir/rechargement sont joués par AStrikeWeapon (FireMontage / ReloadMontage).
 */
UCLASS()
class STRIKEZONE_API UStrikeAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") float Speed = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") float Direction = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") float AimPitch = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") float AimYaw = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") float AimAlpha = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") bool bIsInAir = false;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") bool bIsCrouching = false;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") bool bIsSliding = false;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") bool bIsSprinting = false;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") bool bIsReloading = false;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") bool bIsDead = false;
	UPROPERTY(BlueprintReadOnly, Category = "Strike|Anim") bool bHasWeapon = false;

	UPROPERTY(Transient)
	TObjectPtr<AStrikeCharacter> StrikeCharacter;
};

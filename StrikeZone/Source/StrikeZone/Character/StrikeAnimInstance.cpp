// STRIKE ZONE - base C++ des Animation Blueprints (variables pour State Machines, IK, montages)
#include "Character/StrikeAnimInstance.h"
#include "Character/StrikeCharacter.h"
#include "Weapons/StrikeWeapon.h"
#include "GameFramework/CharacterMovementComponent.h"

void UStrikeAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	StrikeCharacter = Cast<AStrikeCharacter>(TryGetPawnOwner());
}

void UStrikeAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!StrikeCharacter)
	{
		StrikeCharacter = Cast<AStrikeCharacter>(TryGetPawnOwner());
		if (!StrikeCharacter)
		{
			return;
		}
	}

	const FVector Velocity = StrikeCharacter->GetVelocity();
	const FRotator ActorRotation = StrikeCharacter->GetActorRotation();
	Speed = Velocity.Size2D();

	if (Speed > 1.f)
	{
		const FVector LocalVelocity = ActorRotation.UnrotateVector(Velocity);
		Direction = FMath::RadiansToDegrees(FMath::Atan2(LocalVelocity.Y, LocalVelocity.X));
	}
	else
	{
		Direction = 0.f;
	}

	const FRotator AimDelta = (StrikeCharacter->GetBaseAimRotation() - ActorRotation).GetNormalized();
	AimPitch = AimDelta.Pitch;
	AimYaw = AimDelta.Yaw;
	AimAlpha = StrikeCharacter->GetAimAlpha();

	bIsInAir = StrikeCharacter->GetCharacterMovement() && StrikeCharacter->GetCharacterMovement()->IsFalling();
	bIsCrouching = StrikeCharacter->bIsCrouched;
	bIsSliding = StrikeCharacter->IsSliding();
	bIsSprinting = StrikeCharacter->IsSprinting();
	bIsDead = StrikeCharacter->IsDead();

	const AStrikeWeapon* Weapon = StrikeCharacter->GetCurrentWeapon();
	bHasWeapon = Weapon != nullptr;
	bIsReloading = Weapon && Weapon->IsReloading();
}

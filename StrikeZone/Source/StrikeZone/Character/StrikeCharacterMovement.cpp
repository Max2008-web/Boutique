// STRIKE ZONE - mouvement FPS prédit côté client : sprint et glissade
#include "Character/StrikeCharacterMovement.h"
#include "Character/StrikeCharacter.h"
#include "GameFramework/Character.h"

UStrikeCharacterMovement::UStrikeCharacterMovement()
{
	bWantsToSprint = false;
	bWantsToSlide = false;

	MaxWalkSpeed = 450.f;
	MaxWalkSpeedCrouched = 220.f;
	MaxAcceleration = 2600.f;
	BrakingDecelerationWalking = 2048.f;
	GroundFriction = 8.f;
	JumpZVelocity = 460.f;
	AirControl = 0.35f;
	GravityScale = 1.3f;
	bCanWalkOffLedgesWhenCrouching = true;
	NavAgentProps.bCanCrouch = true;
	NavAgentProps.bCanJump = true;
	SetCrouchedHalfHeight(48.f);
}

void UStrikeCharacterMovement::BeginPlay()
{
	Super::BeginPlay();
	DefaultGroundFriction = GroundFriction;
	DefaultBrakingDeceleration = BrakingDecelerationWalking;
}

bool UStrikeCharacterMovement::IsSprintingNow() const
{
	return bWantsToSprint && !bIsSlidingInternal && IsMovingOnGround() && !IsCrouching() && Velocity.SizeSquared2D() > FMath::Square(MaxWalkSpeed * 0.8f);
}

float UStrikeCharacterMovement::GetMaxSpeed() const
{
	if (bIsSlidingInternal)
	{
		return SprintSpeed + SlideImpulse;
	}

	if (MovementMode == MOVE_Walking || MovementMode == MOVE_NavWalking)
	{
		const AStrikeCharacter* StrikeOwner = Cast<AStrikeCharacter>(CharacterOwner);
		const bool bAiming = StrikeOwner && StrikeOwner->IsAiming();

		if (!IsCrouching())
		{
			if (bAiming)
			{
				return AimingSpeed;
			}

			// Le sprint ne s'applique qu'en avançant (évite le sprint latéral/arrière).
			if (bWantsToSprint && CharacterOwner)
			{
				const FVector Forward = CharacterOwner->GetActorForwardVector();
				const FVector AccelDir = GetCurrentAcceleration().GetSafeNormal2D();
				if (FVector::DotProduct(Forward, AccelDir) > 0.5f)
				{
					return SprintSpeed;
				}
			}
		}
	}

	return Super::GetMaxSpeed();
}

float UStrikeCharacterMovement::GetMaxAcceleration() const
{
	// Pendant la glissade, l'entrée du joueur n'oriente que faiblement la trajectoire.
	return bIsSlidingInternal ? MaxAcceleration * 0.1f : Super::GetMaxAcceleration();
}

void UStrikeCharacterMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToSlide = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
}

FNetworkPredictionData_Client* UStrikeCharacterMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UStrikeCharacterMovement* MutableThis = const_cast<UStrikeCharacterMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Strike(*this);
	}
	return ClientPredictionData;
}

void UStrikeCharacterMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (bIsSlidingInternal)
		{
			SlideTimeRemaining -= DeltaSeconds;
			if (!bWantsToSlide || SlideTimeRemaining <= 0.f || !IsMovingOnGround() || Velocity.Size2D() < SlideExitSpeed)
			{
				StopSlide();
			}
		}
		else
		{
			SlideCooldownRemaining = FMath::Max(0.f, SlideCooldownRemaining - DeltaSeconds);
			if (bWantsToSlide && SlideCooldownRemaining <= 0.f && IsMovingOnGround() && Velocity.Size2D() >= SlideMinSpeed)
			{
				StartSlide();
			}
		}
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UStrikeCharacterMovement::StartSlide()
{
	bIsSlidingInternal = true;
	SlideTimeRemaining = SlideDuration;
	Velocity += Velocity.GetSafeNormal2D() * SlideImpulse;
	GroundFriction = SlideGroundFriction;
	BrakingDecelerationWalking = SlideBrakingDeceleration;
	bWantsToCrouch = true;
}

void UStrikeCharacterMovement::StopSlide()
{
	bIsSlidingInternal = false;
	SlideCooldownRemaining = SlideCooldown;
	GroundFriction = DefaultGroundFriction;
	BrakingDecelerationWalking = DefaultBrakingDeceleration;
}

// ---------------------------------------------------------------------------
// FSavedMove_Strike
// ---------------------------------------------------------------------------

void FSavedMove_Strike::Clear()
{
	Super::Clear();
	bSavedWantsToSprint = false;
	bSavedWantsToSlide = false;
}

uint8 FSavedMove_Strike::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();
	if (bSavedWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}
	if (bSavedWantsToSlide)
	{
		Result |= FLAG_Custom_1;
	}
	return Result;
}

bool FSavedMove_Strike::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	const FSavedMove_Strike* Other = static_cast<const FSavedMove_Strike*>(NewMove.Get());
	if (bSavedWantsToSprint != Other->bSavedWantsToSprint || bSavedWantsToSlide != Other->bSavedWantsToSlide)
	{
		return false;
	}
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Strike::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);
	if (const UStrikeCharacterMovement* Move = Cast<UStrikeCharacterMovement>(C->GetCharacterMovement()))
	{
		bSavedWantsToSprint = Move->bWantsToSprint;
		bSavedWantsToSlide = Move->bWantsToSlide;
	}
}

void FSavedMove_Strike::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	if (UStrikeCharacterMovement* Move = Cast<UStrikeCharacterMovement>(C->GetCharacterMovement()))
	{
		Move->bWantsToSprint = bSavedWantsToSprint;
		Move->bWantsToSlide = bSavedWantsToSlide;
	}
}

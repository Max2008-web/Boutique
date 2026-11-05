// STRIKE ZONE - mouvement FPS prédit côté client : sprint et glissade
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "StrikeCharacterMovement.generated.h"

/**
 * Composant de mouvement avec sprint et glissade intégrés à la prédiction réseau
 * (drapeaux compressés FLAG_Custom_0 / FLAG_Custom_1). Le serveur rejoue les mouvements
 * du client avec les mêmes entrées : aucune vitesse n'est imposée par le client.
 */
UCLASS()
class STRIKEZONE_API UStrikeCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_Strike;

public:
	UStrikeCharacterMovement();

	// --- Réglages ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Movement")
	float SprintSpeed = 700.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Movement")
	float AimingSpeed = 280.f;

	/** Vitesse minimale pour déclencher une glissade. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Slide")
	float SlideMinSpeed = 520.f;

	/** Vitesse sous laquelle la glissade s'arrête. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Slide")
	float SlideExitSpeed = 260.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Slide")
	float SlideImpulse = 380.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Slide")
	float SlideDuration = 0.85f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Slide")
	float SlideGroundFriction = 0.35f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Slide")
	float SlideBrakingDeceleration = 450.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Strike|Slide")
	float SlideCooldown = 0.6f;

	// --- Requêtes (posées localement, transmises via les drapeaux compressés) ---
	void SetWantsToSprint(bool bWants) { bWantsToSprint = bWants; }
	void SetWantsToSlide(bool bWants) { bWantsToSlide = bWants; }
	bool WantsToSprint() const { return bWantsToSprint; }

	UFUNCTION(BlueprintPure, Category = "Strike|Movement")
	bool IsSliding() const { return bIsSlidingInternal; }

	UFUNCTION(BlueprintPure, Category = "Strike|Movement")
	bool IsSprintingNow() const;

	// --- UCharacterMovementComponent ---
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	void StartSlide();
	void StopSlide();

	uint8 bWantsToSprint : 1;
	uint8 bWantsToSlide : 1;

	bool bIsSlidingInternal = false;
	float SlideTimeRemaining = 0.f;
	float SlideCooldownRemaining = 0.f;

	float DefaultGroundFriction = 8.f;
	float DefaultBrakingDeceleration = 2048.f;
};

/** Mouvement sauvegardé contenant les intentions sprint/glissade. */
class FSavedMove_Strike : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	uint8 bSavedWantsToSprint : 1;
	uint8 bSavedWantsToSlide : 1;

	FSavedMove_Strike() : bSavedWantsToSprint(0), bSavedWantsToSlide(0) {}

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
};

class FNetworkPredictionData_Client_Strike : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	explicit FNetworkPredictionData_Client_Strike(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override
	{
		return FSavedMovePtr(new FSavedMove_Strike());
	}
};

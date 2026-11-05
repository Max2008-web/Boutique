// STRIKE ZONE - traceur de balle purement cosmétique (local, non répliqué)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StrikeTracer.generated.h"

class UStaticMeshComponent;

UCLASS(NotBlueprintable)
class STRIKEZONE_API AStrikeTracer : public AActor
{
	GENERATED_BODY()

public:
	AStrikeTracer();

	void InitTracer(const FVector& Start, const FVector& End);
	virtual void Tick(float DeltaSeconds) override;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Components")
	TObjectPtr<UStaticMeshComponent> Mesh;

	FVector Direction = FVector::ForwardVector;
	float RemainingDistance = 0.f;
	float Speed = 25000.f;
};

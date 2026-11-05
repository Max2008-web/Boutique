// STRIKE ZONE - traceur de balle purement cosmétique (local, non répliqué)
#include "Weapons/StrikeTracer.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AStrikeTracer::AStrikeTracer()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	InitialLifeSpan = 1.f;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);
	Mesh->SetRelativeScale3D(FVector(0.9f, 0.015f, 0.015f));
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}
}

void AStrikeTracer::InitTracer(const FVector& Start, const FVector& End)
{
	const FVector Delta = End - Start;
	RemainingDistance = Delta.Size();
	Direction = Delta.GetSafeNormal();
	SetActorLocationAndRotation(Start + Direction * 50.f, Direction.Rotation());

	if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), FLinearColor(1.f, 0.85f, 0.35f));
	}
	if (RemainingDistance < 100.f)
	{
		Destroy();
	}
}

void AStrikeTracer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Step = Speed * DeltaSeconds;
	RemainingDistance -= Step;
	if (RemainingDistance <= 0.f)
	{
		Destroy();
		return;
	}
	SetActorLocation(GetActorLocation() + Direction * Step);
}

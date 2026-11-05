// STRIKE ZONE - projectiles physiques (roquettes, grenades)
#include "Weapons/StrikeProjectile.h"
#include "Components/HealthComponent.h"
#include "Core/StrikeTypes.h"

#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/DamageType.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

AStrikeProjectile::AStrikeProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);
	InitialLifeSpan = 6.f;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(8.f);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Collision->SetCollisionResponseToAllChannels(ECR_Block);
	Collision->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCanEverAffectNavigation(false);
	Collision->bReturnMaterialOnMove = false;
	RootComponent = Collision;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Collision);
	Visual->SetRelativeScale3D(FVector(0.16f));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	if (SphereMesh.Succeeded())
	{
		Visual->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Visual->SetMaterial(0, ShapeMaterial.Object);
	}

	Glow = CreateDefaultSubobject<UPointLightComponent>(TEXT("Glow"));
	Glow->SetupAttachment(Collision);
	Glow->SetIntensityUnits(ELightUnits::Candelas);
	Glow->SetIntensity(80.f);
	Glow->SetAttenuationRadius(250.f);
	Glow->SetLightColor(FLinearColor(1.f, 0.55f, 0.2f));
	Glow->SetCastShadows(false);

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->InitialSpeed = 5000.f;
	Movement->MaxSpeed = 5000.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->ProjectileGravityScale = 0.15f;
	Movement->bShouldBounce = false;
}

void AStrikeProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (APawn* InstigatorPawn = GetInstigator())
	{
		Collision->IgnoreActorWhenMoving(InstigatorPawn, true);
	}
	if (UMaterialInstanceDynamic* MID = Visual->CreateDynamicMaterialInstance(0))
	{
		MID->SetVectorParameterValue(TEXT("Color"), PlaceholderColor);
	}
	if (HasAuthority())
	{
		Collision->OnComponentHit.AddDynamic(this, &AStrikeProjectile::OnProjectileHit);
	}
}

void AStrikeProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeProjectile, bExploded);
	DOREPLIFETIME(AStrikeProjectile, ExplosionLocation);
}

void AStrikeProjectile::InitProjectile(float InDirectDamage, const FString& InWeaponName)
{
	DirectDamage = InDirectDamage;
	WeaponName = InWeaponName;
}

void AStrikeProjectile::OnProjectileHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!HasAuthority() || bExploded)
	{
		return;
	}

	AController* InstigatorController = GetInstigatorController();

	if (OtherActor && OtherActor != GetInstigator() && OtherActor->CanBeDamaged() && DirectDamage > 0.f)
	{
		const EStrikeHitZone Zone = UHealthComponent::GetHitZoneFromHit(Hit, OtherActor);
		const float ZoneMul = (Zone == EStrikeHitZone::Head) ? 1.5f : 1.f;
		UGameplayStatics::ApplyPointDamage(OtherActor, DirectDamage * ZoneMul, GetVelocity().GetSafeNormal(), Hit, InstigatorController, this, UDamageType::StaticClass());
	}

	if (ExplosionRadius > 0.f && ExplosionDamage > 0.f)
	{
		TArray<AActor*> Ignore;
		UGameplayStatics::ApplyRadialDamageWithFalloff(this, ExplosionDamage, ExplosionDamage * 0.15f, Hit.ImpactPoint,
			ExplosionRadius * 0.3f, ExplosionRadius, 1.f, UDamageType::StaticClass(), Ignore, this, InstigatorController, ECC_Visibility);
	}

	ExplosionLocation = Hit.ImpactPoint;
	bExploded = true;
	Movement->StopMovementImmediately();
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetActorHiddenInGame(true);
	PlayExplosionCosmetics();

	// Laisse le temps à bExploded de se répliquer avant la destruction.
	SetLifeSpan(0.5f);
}

void AStrikeProjectile::OnRep_Exploded()
{
	if (bExploded)
	{
		Movement->StopMovementImmediately();
		SetActorHiddenInGame(true);
		PlayExplosionCosmetics();
	}
}

void AStrikeProjectile::PlayExplosionCosmetics()
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (ExplosionFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionFX, ExplosionLocation);
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, ExplosionLocation);
	}
}

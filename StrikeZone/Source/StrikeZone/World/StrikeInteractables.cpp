// STRIKE ZONE - achats muraux (Zombies), zones d'objectif (Tactique/Extraction), cibles (Entraînement), zone BR
#include "World/StrikeInteractables.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/WeaponComponent.h"
#include "Game/StrikeGameMode.h"
#include "Game/StrikeGameState.h"
#include "Game/StrikePlayerState.h"
#include "Weapons/StrikeWeapon.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

namespace StrikeShapes
{
	static UStaticMeshComponent* MakeShape(AActor* Owner, const TCHAR* Name, const TCHAR* MeshPath)
	{
		UStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(MeshPath);
		ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
		if (MeshFinder.Succeeded())
		{
			Comp->SetStaticMesh(MeshFinder.Object);
		}
		if (MaterialFinder.Succeeded())
		{
			Comp->SetMaterial(0, MaterialFinder.Object);
		}
		Comp->SetCanEverAffectNavigation(false);
		return Comp;
	}

	static void Tint(UStaticMeshComponent* Comp, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	}
}

// ===========================================================================
// AStrikeWeaponBuy
// ===========================================================================

AStrikeWeaponBuy::AStrikeWeaponBuy()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Panel = StrikeShapes::MakeShape(this, TEXT("Panel"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	RootComponent = Panel;
	Panel->SetRelativeScale3D(FVector(0.1f, 1.6f, 1.0f));
	Panel->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	WeaponPreview = StrikeShapes::MakeShape(this, TEXT("WeaponPreview"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	WeaponPreview->SetupAttachment(Panel);
	WeaponPreview->SetUsingAbsoluteScale(true);
	WeaponPreview->SetWorldScale3D(FVector(0.12f, 0.7f, 0.12f));
	WeaponPreview->SetRelativeLocation(FVector(70.f, 0.f, 0.f));
	WeaponPreview->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AStrikeWeaponBuy::BeginPlay()
{
	Super::BeginPlay();
	StrikeShapes::Tint(Panel, FLinearColor(0.25f, 0.2f, 0.12f));
	StrikeShapes::Tint(WeaponPreview, FLinearColor(0.9f, 0.7f, 0.2f));
}

void AStrikeWeaponBuy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeWeaponBuy, WeaponClass);
	DOREPLIFETIME(AStrikeWeaponBuy, Price);
}

void AStrikeWeaponBuy::Setup(TSubclassOf<AStrikeWeapon> InWeaponClass, int32 InPrice)
{
	WeaponClass = InWeaponClass;
	Price = InPrice;
}

int32 AStrikeWeaponBuy::GetPriceFor(const AStrikeCharacter* User) const
{
	const bool bOwned = User && User->GetWeaponComponent()->FindWeaponOfClass(WeaponClass) != nullptr;
	return bOwned ? Price / 2 : Price;
}

bool AStrikeWeaponBuy::CanInteract(const AStrikeCharacter* User) const
{
	return User && !User->IsDead() && WeaponClass && User->GetPlayerState();
}

FString AStrikeWeaponBuy::GetInteractPrompt(const AStrikeCharacter* User) const
{
	const AStrikeWeapon* CDO = WeaponClass ? WeaponClass->GetDefaultObject<AStrikeWeapon>() : nullptr;
	const bool bOwned = User && User->GetWeaponComponent()->FindWeaponOfClass(WeaponClass) != nullptr;
	return FString::Printf(TEXT("%s %s [%d pts]"), bOwned ? TEXT("Munitions") : TEXT("Acheter"), CDO ? *CDO->WeaponName : TEXT("?"), GetPriceFor(User));
}

void AStrikeWeaponBuy::Interact(AStrikeCharacter* User)
{
	if (!HasAuthority() || !CanInteract(User))
	{
		return;
	}
	AStrikePlayerState* PS = User->GetPlayerState<AStrikePlayerState>();
	if (PS && PS->SpendPoints(GetPriceFor(User)))
	{
		User->GetWeaponComponent()->GiveWeapon(WeaponClass, true);
	}
}

// ===========================================================================
// AStrikeObjectiveZone
// ===========================================================================

AStrikeObjectiveZone::AStrikeObjectiveZone()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	Disk = StrikeShapes::MakeShape(this, TEXT("Disk"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	RootComponent = Disk;
	Disk->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disk->SetCastShadow(false);

	Beacon = StrikeShapes::MakeShape(this, TEXT("Beacon"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	Beacon->SetupAttachment(Disk);
	Beacon->SetUsingAbsoluteScale(true);
	Beacon->SetWorldScale3D(FVector(0.2f, 0.2f, 8.f));
	Beacon->SetRelativeLocation(FVector(0.f, 0.f, 400.f));
	Beacon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Beacon->SetCastShadow(false);
}

void AStrikeObjectiveZone::BeginPlay()
{
	Super::BeginPlay();
	DiskMaterial = Disk->CreateDynamicMaterialInstance(0);
	Beacon->SetMaterial(0, DiskMaterial);
	RefreshVisual();
}

void AStrikeObjectiveZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeObjectiveZone, ObjectiveType);
	DOREPLIFETIME(AStrikeObjectiveZone, Label);
	DOREPLIFETIME(AStrikeObjectiveZone, Radius);
	DOREPLIFETIME(AStrikeObjectiveZone, bChargePlanted);
	DOREPLIFETIME(AStrikeObjectiveZone, bZoneActive);
}

void AStrikeObjectiveZone::SetZoneActive(bool bNewActive)
{
	bZoneActive = bNewActive;
	RefreshVisual();
}

void AStrikeObjectiveZone::OnRep_Visual()
{
	RefreshVisual();
}

void AStrikeObjectiveZone::RefreshVisual()
{
	// Cylindre moteur : 100 cm de diamètre et de hauteur.
	Disk->SetWorldScale3D(FVector(Radius / 50.f, Radius / 50.f, 0.05f));
	Beacon->SetVisibility(bZoneActive);
	if (DiskMaterial)
	{
		FLinearColor Color = ObjectiveType == EStrikeObjectiveType::Extraction ? FLinearColor(0.1f, 0.9f, 0.3f) : FLinearColor(0.95f, 0.6f, 0.05f);
		if (!bZoneActive)
		{
			Color = FLinearColor(0.2f, 0.2f, 0.2f);
		}
		if (bChargePlanted)
		{
			Color = FLinearColor(1.f, 0.05f, 0.05f);
		}
		DiskMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}
}

bool AStrikeObjectiveZone::IsInside(const AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}
	const FVector Delta = Actor->GetActorLocation() - GetActorLocation();
	return FVector2D(Delta).SizeSquared() <= FMath::Square(Radius) && FMath::Abs(Delta.Z) < 400.f;
}

TArray<AStrikeCharacter*> AStrikeObjectiveZone::GetCharactersInside() const
{
	TArray<AStrikeCharacter*> Result;
	for (TActorIterator<AStrikeCharacter> It(GetWorld()); It; ++It)
	{
		if (!It->IsDead() && IsInside(*It))
		{
			Result.Add(*It);
		}
	}
	return Result;
}

// ===========================================================================
// AStrikeTarget
// ===========================================================================

AStrikeTarget::AStrikeTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(true);

	Pivot = CreateDefaultSubobject<USceneComponent>(TEXT("Pivot"));
	RootComponent = Pivot;

	Stand = StrikeShapes::MakeShape(this, TEXT("Stand"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	Stand->SetupAttachment(Pivot);
	Stand->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.1f));
	Stand->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	Stand->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	Body = StrikeShapes::MakeShape(this, TEXT("Body"), TEXT("/Engine/BasicShapes/Cube.Cube"));
	Body->SetupAttachment(Pivot);
	Body->SetRelativeScale3D(FVector(0.1f, 0.55f, 1.2f));
	Body->SetRelativeLocation(FVector(0.f, 0.f, 80.f));
	Body->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Body->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);

	Head = StrikeShapes::MakeShape(this, TEXT("Head"), TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Head->SetupAttachment(Pivot);
	Head->SetRelativeScale3D(FVector(0.3f));
	Head->SetRelativeLocation(FVector(0.f, 0.f, 160.f));
	Head->SetCollisionProfileName(TEXT("BlockAllDynamic"));
	Head->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);
	Head->ComponentTags.Add(TEXT("Head"));
}

void AStrikeTarget::BeginPlay()
{
	Super::BeginPlay();
	Origin = GetActorLocation();
	Health = MaxHealth;
	StrikeShapes::Tint(Stand, FLinearColor(0.2f, 0.2f, 0.2f));
	StrikeShapes::Tint(Body, FLinearColor(0.85f, 0.85f, 0.8f));
	StrikeShapes::Tint(Head, FLinearColor(0.9f, 0.15f, 0.1f));
}

void AStrikeTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeTarget, bDown);
	DOREPLIFETIME(AStrikeTarget, MoveAmplitude);
	DOREPLIFETIME(AStrikeTarget, MoveSpeed);
}

void AStrikeTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HasAuthority() && MoveAmplitude > 0.f && !bDown)
	{
		const float Offset = FMath::Sin(GetWorld()->GetTimeSeconds() * MoveSpeed) * MoveAmplitude;
		SetActorLocation(Origin + GetActorRightVector() * Offset);
	}
}

float AStrikeTarget::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || bDown)
	{
		return 0.f;
	}
	const float Damage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	EStrikeHitZone Zone = EStrikeHitZone::Torso;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		Zone = UHealthComponent::GetHitZoneFromHit(static_cast<const FPointDamageEvent&>(DamageEvent).HitInfo, this);
	}
	if (AStrikeGameMode* GM = GetWorld()->GetAuthGameMode<AStrikeGameMode>())
	{
		GM->NotifyDamageDealt(EventInstigator, this, Damage, Zone);
	}

	Health -= Damage;
	if (Health <= 0.f)
	{
		bDown = true;
		OnRep_Down();
		GetWorldTimerManager().SetTimer(ResetHandle, this, &AStrikeTarget::ResetTarget, ResetDelay, false);
	}
	return Damage;
}

void AStrikeTarget::ResetTarget()
{
	bDown = false;
	Health = MaxHealth;
	OnRep_Down();
}

void AStrikeTarget::OnRep_Down()
{
	// La cible bascule en arrière lorsqu'elle est "éliminée".
	Body->SetRelativeRotation(FRotator(bDown ? -80.f : 0.f, 0.f, 0.f));
	Head->SetVisibility(!bDown);
	Head->SetCollisionEnabled(bDown ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionEnabled(bDown ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}

// ===========================================================================
// AStrikeStormZone
// ===========================================================================

AStrikeStormZone::AStrikeStormZone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bAlwaysRelevant = true;

	Wall = StrikeShapes::MakeShape(this, TEXT("Wall"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	RootComponent = Wall;
	Wall->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Wall->SetCastShadow(false);
}

void AStrikeStormZone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	UMaterialInterface* WireMaterial = GEngine ? GEngine->WireframeMaterial.Get() : nullptr;
	if (WireMaterial && Wall->GetMaterial(0) != WireMaterial)
	{
		// Matériau fil de fer du moteur : mur de zone visible sans bloquer la vue.
		// (Sur les GPU mobiles sans rendu fil de fer, la face interne reste invisible grâce au backface culling.)
		Wall->SetMaterial(0, WireMaterial);
	}

	const AStrikeGameState* GS = GetWorld()->GetGameState<AStrikeGameState>();
	if (!GS || GS->ZoneRadius <= 0.f)
	{
		Wall->SetVisibility(false);
		return;
	}
	Wall->SetVisibility(true);
	DisplayedRadius = FMath::FInterpTo(DisplayedRadius, GS->ZoneRadius, DeltaSeconds, 2.f);
	const float Height = 4000.f;
	Wall->SetWorldLocation(FVector(GS->ZoneCenter.X, GS->ZoneCenter.Y, Height * 0.5f - 200.f));
	Wall->SetWorldScale3D(FVector(DisplayedRadius / 50.f, DisplayedRadius / 50.f, Height / 100.f));
}

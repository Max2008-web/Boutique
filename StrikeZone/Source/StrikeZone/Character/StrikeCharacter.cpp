// STRIKE ZONE - personnage FPS jouable (joueurs, bots, base des zombies)
#include "Character/StrikeCharacter.h"
#include "Character/StrikeCharacterMovement.h"
#include "Components/HealthComponent.h"
#include "Components/WeaponComponent.h"
#include "Components/InventoryComponent.h"
#include "Weapons/StrikeWeapon.h"
#include "Game/StrikeGameMode.h"
#include "Game/StrikePlayerState.h"
#include "AI/StrikeAIController.h"
#include "StrikeZone.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace StrikeBody
{
	static const FVector StandBodyScale(0.7f, 0.7f, 1.45f);
	static const FVector StandBodyLoc(0.f, 0.f, -15.f);
	static const FVector StandHeadLoc(0.f, 0.f, 70.f);
	static const FVector CrouchBodyScale(0.7f, 0.7f, 0.8f);
	static const FVector CrouchBodyLoc(0.f, 0.f, -8.f);
	static const FVector CrouchHeadLoc(0.f, 0.f, 42.f);
}

AStrikeCharacter::AStrikeCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UStrikeCharacterMovement>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(38.f, 88.f);
	GetCapsuleComponent()->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Block);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	BaseEyeHeight = 64.f;
	CrouchedEyeHeight = 40.f;
	CurrentEyeZ = BaseEyeHeight;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetFieldOfView(BaseFieldOfView);

	WeaponAttachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponAttachPoint"));
	WeaponAttachPoint->SetupAttachment(FirstPersonCamera);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	// Corps temporaire : remplacé par GetMesh() (Skeletal Mesh + Animation Blueprint) dès qu'un asset est fourni.
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(GetCapsuleComponent());
	BodyMesh->SetRelativeScale3D(StrikeBody::StandBodyScale);
	BodyMesh->SetRelativeLocation(StrikeBody::StandBodyLoc);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMesh->SetOwnerNoSee(true);
	BodyMesh->bCastHiddenShadow = true;
	BodyMesh->SetCanEverAffectNavigation(false);

	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(GetCapsuleComponent());
	HeadMesh->SetRelativeScale3D(FVector(0.38f));
	HeadMesh->SetRelativeLocation(StrikeBody::StandHeadLoc);
	HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMesh->SetOwnerNoSee(true);
	HeadMesh->bCastHiddenShadow = true;
	HeadMesh->SetCanEverAffectNavigation(false);

	if (CylinderMesh.Succeeded())
	{
		BodyMesh->SetStaticMesh(CylinderMesh.Object);
	}
	if (SphereMesh.Succeeded())
	{
		HeadMesh->SetStaticMesh(SphereMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		BodyMesh->SetMaterial(0, ShapeMaterial.Object);
		HeadMesh->SetMaterial(0, ShapeMaterial.Object);
	}

	GetMesh()->SetOwnerNoSee(true);
	GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, -88.f));
	GetMesh()->SetRelativeRotation(FRotator(0.f, -90.f, 0.f));

	HealthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
	InventoryComponent = CreateDefaultSubobject<UInventoryComponent>(TEXT("InventoryComponent"));

	// Les pions des joueurs et des bots sont possédés par le GameMode (RestartPlayer) :
	// pas de contrôleur IA automatique, sauf pour les personnages placés à la main dans un niveau.
	AIControllerClass = AStrikeAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorld;

	SetNetUpdateFrequency(60.f);
}

void AStrikeCharacter::BeginPlay()
{
	Super::BeginPlay();

	BodyMaterial = BodyMesh->CreateDynamicMaterialInstance(0);
	if (BodyMaterial)
	{
		HeadMesh->SetMaterial(0, BodyMaterial);
	}
	ApplyTeamColor();

	HealthComponent->OnDeath.AddDynamic(this, &AStrikeCharacter::HandleDeath);
	CurrentEyeZ = BaseEyeHeight;
}

UStrikeCharacterMovement* AStrikeCharacter::GetStrikeMovement() const
{
	return Cast<UStrikeCharacterMovement>(GetCharacterMovement());
}

AStrikeWeapon* AStrikeCharacter::GetCurrentWeapon() const
{
	return WeaponComponent ? WeaponComponent->GetCurrentWeapon() : nullptr;
}

bool AStrikeCharacter::IsSprinting() const
{
	const UStrikeCharacterMovement* Move = GetStrikeMovement();
	if (IsLocallyControlled())
	{
		return Move && Move->IsSprintingNow();
	}
	// Proxys simulés : déduit de la vitesse (le drapeau sprint n'est pas répliqué aux autres clients).
	return Move && Move->IsMovingOnGround() && !bIsCrouched && GetVelocity().Size2D() > Move->MaxWalkSpeed * 1.15f;
}

bool AStrikeCharacter::IsSliding() const
{
	const UStrikeCharacterMovement* Move = GetStrikeMovement();
	if (Move && Move->IsSliding())
	{
		return true;
	}
	return !IsLocallyControlled() && bIsCrouched && GetVelocity().Size2D() > 450.f;
}

bool AStrikeCharacter::IsDead() const
{
	return HealthComponent && HealthComponent->IsDead();
}

void AStrikeCharacter::SetTeamId(uint8 NewTeam)
{
	TeamId = NewTeam;
	ApplyTeamColor();
}

void AStrikeCharacter::OnRep_TeamId()
{
	ApplyTeamColor();
}

void AStrikeCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();
	ApplyTeamColor();
}

void AStrikeCharacter::ApplyTeamColor()
{
	if (BodyMaterial)
	{
		// "Color" est le paramètre vectoriel du matériau BasicShapeMaterial du moteur.
		BodyMaterial->SetVectorParameterValue(TEXT("Color"), StrikeUtils::TeamColor(TeamId));
	}
}

void AStrikeCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	uint8 NewTeam = TeamId;
	if (const AStrikePlayerState* PS = NewController ? NewController->GetPlayerState<AStrikePlayerState>() : nullptr)
	{
		NewTeam = PS->GetTeamId();
	}
	else if (const AStrikeAIController* AI = Cast<AStrikeAIController>(NewController))
	{
		NewTeam = AI->GetTeamId();
	}
	SetTeamId(NewTeam);
}

void AStrikeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(AStrikeCharacter, bIsAiming, COND_SkipOwner);
	DOREPLIFETIME(AStrikeCharacter, TeamId);
}

void AStrikeCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const AStrikeWeapon* Weapon = GetCurrentWeapon();
	const float AimSpeed = Weapon ? Weapon->AimSpeed : 8.f;
	const bool bCanAim = bIsAiming && !IsDead() && !(Weapon && Weapon->IsReloading());
	AimAlpha = FMath::FInterpConstantTo(AimAlpha, bCanAim ? 1.f : 0.f, DeltaSeconds, AimSpeed);

	UpdateCamera(DeltaSeconds);
}

void AStrikeCharacter::UpdateCamera(float DeltaSeconds)
{
	CurrentEyeZ = FMath::FInterpTo(CurrentEyeZ, BaseEyeHeight, DeltaSeconds, 12.f);
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, CurrentEyeZ));

	const bool bLocalPlayer = IsLocallyControlled() && IsPlayerControlled();
	if (bLocalPlayer)
	{
		const AStrikeWeapon* Weapon = GetCurrentWeapon();
		const float TargetFOV = Weapon ? FMath::Lerp(BaseFieldOfView, Weapon->AimFOV, AimAlpha) : BaseFieldOfView;
		FirstPersonCamera->SetFieldOfView(TargetFOV);
	}
	else
	{
		// La caméra ne suit la rotation de contrôle que pour le joueur local :
		// pour les autres (proxys, IA), on oriente l'attache d'arme avec la visée répliquée.
		FirstPersonCamera->SetWorldRotation(GetBaseAimRotation());
	}
}

void AStrikeCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CurrentEyeZ += HalfHeightAdjust;
	BodyMesh->SetRelativeScale3D(StrikeBody::CrouchBodyScale);
	BodyMesh->SetRelativeLocation(StrikeBody::CrouchBodyLoc);
	HeadMesh->SetRelativeLocation(StrikeBody::CrouchHeadLoc);
}

void AStrikeCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
	Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
	CurrentEyeZ -= HalfHeightAdjust;
	BodyMesh->SetRelativeScale3D(StrikeBody::StandBodyScale);
	BodyMesh->SetRelativeLocation(StrikeBody::StandBodyLoc);
	HeadMesh->SetRelativeLocation(StrikeBody::StandHeadLoc);
}

bool AStrikeCharacter::CanJumpInternal_Implementation() const
{
	if (IsDead())
	{
		return false;
	}
	// Autorise le saut accroupi / en fin de glissade.
	return bIsCrouched ? JumpIsAllowedInternal() : Super::CanJumpInternal_Implementation();
}

// ---------------------------------------------------------------------------
// Entrées
// ---------------------------------------------------------------------------

void AStrikeCharacter::MoveInput(const FVector2D& Axis)
{
	if (IsDead())
	{
		return;
	}
	AddMovementInput(GetActorForwardVector(), Axis.Y);
	AddMovementInput(GetActorRightVector(), Axis.X);
}

void AStrikeCharacter::StartJump()
{
	if (bIsCrouched)
	{
		bCrouchToggled = false;
		if (UStrikeCharacterMovement* Move = GetStrikeMovement())
		{
			Move->SetWantsToSlide(false);
		}
		UnCrouch();
	}
	Jump();
}

void AStrikeCharacter::StopJump()
{
	StopJumping();
}

void AStrikeCharacter::StartCrouchOrSlide()
{
	if (IsDead())
	{
		return;
	}
	if (UStrikeCharacterMovement* Move = GetStrikeMovement())
	{
		Move->SetWantsToSlide(Move->IsSprintingNow());
	}
	Crouch();
}

void AStrikeCharacter::StopCrouchOrSlide()
{
	if (UStrikeCharacterMovement* Move = GetStrikeMovement())
	{
		Move->SetWantsToSlide(false);
	}
	UnCrouch();
}

void AStrikeCharacter::ToggleCrouch()
{
	bCrouchToggled = !bCrouchToggled;
	if (bCrouchToggled)
	{
		StartCrouchOrSlide();
	}
	else
	{
		StopCrouchOrSlide();
	}
}

void AStrikeCharacter::SetSprinting(bool bNewSprinting)
{
	if (bNewSprinting)
	{
		if (bIsAiming)
		{
			SetAiming(false);
		}
		StopFire();
		if (bIsCrouched && !IsSliding())
		{
			bCrouchToggled = false;
			StopCrouchOrSlide();
		}
	}
	if (UStrikeCharacterMovement* Move = GetStrikeMovement())
	{
		Move->SetWantsToSprint(bNewSprinting);
	}
}

void AStrikeCharacter::StartFire()
{
	if (IsDead())
	{
		return;
	}
	if (UStrikeCharacterMovement* Move = GetStrikeMovement())
	{
		Move->SetWantsToSprint(false);
	}
	WeaponComponent->StartFire();
}

void AStrikeCharacter::StopFire()
{
	if (WeaponComponent)
	{
		WeaponComponent->StopFire();
	}
}

void AStrikeCharacter::SetAiming(bool bNewAiming)
{
	if (IsDead())
	{
		bNewAiming = false;
	}
	if (bNewAiming)
	{
		if (UStrikeCharacterMovement* Move = GetStrikeMovement())
		{
			Move->SetWantsToSprint(false);
		}
	}
	if (bIsAiming == bNewAiming)
	{
		return;
	}
	bIsAiming = bNewAiming;
	if (!HasAuthority())
	{
		ServerSetAiming(bNewAiming);
	}
}

void AStrikeCharacter::ServerSetAiming_Implementation(bool bNewAiming)
{
	bIsAiming = bNewAiming && !IsDead();
}

void AStrikeCharacter::ReloadWeapon()
{
	if (!IsDead())
	{
		WeaponComponent->StartReload();
	}
}

void AStrikeCharacter::EquipSlot(int32 SlotIndex)
{
	if (!IsDead())
	{
		WeaponComponent->EquipWeaponAtIndex(SlotIndex);
	}
}

void AStrikeCharacter::CycleWeapon(int32 Direction)
{
	if (!IsDead())
	{
		WeaponComponent->CycleWeapon(Direction);
	}
}

void AStrikeCharacter::Interact()
{
	if (IsDead())
	{
		return;
	}
	if (HasAuthority())
	{
		ServerInteract_Implementation();
	}
	else
	{
		ServerInteract();
	}
}

void AStrikeCharacter::ServerInteract_Implementation()
{
	if (IsDead())
	{
		return;
	}
	if (AActor* Target = FindInteractable())
	{
		if (IStrikeInteractable* Interactable = Cast<IStrikeInteractable>(Target))
		{
			Interactable->Interact(this);
		}
	}
}

void AStrikeCharacter::SetInteractHeld(bool bHeld)
{
	if (HasAuthority())
	{
		bInteractHeld = bHeld;
	}
	else
	{
		ServerSetInteractHeld(bHeld);
	}
}

void AStrikeCharacter::ServerSetInteractHeld_Implementation(bool bHeld)
{
	bInteractHeld = bHeld && !IsDead();
}

AActor* AStrikeCharacter::FindInteractable() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(StrikeInteract), false, this);
	if (const AStrikeWeapon* Weapon = GetCurrentWeapon())
	{
		Params.AddIgnoredActor(Weapon);
	}

	// 1) Objet visé.
	const FVector Start = FirstPersonCamera->GetComponentLocation();
	const FVector End = Start + GetBaseAimRotation().Vector() * InteractRange;
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		if (const IStrikeInteractable* Interactable = Cast<IStrikeInteractable>(Hit.GetActor()))
		{
			if (Interactable->CanInteract(this))
			{
				return Hit.GetActor();
			}
		}
	}

	// 2) Objet le plus proche autour du joueur.
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Visibility, FCollisionShape::MakeSphere(InteractRange * 0.6f), Params);

	AActor* Best = nullptr;
	float BestDistSq = TNumericLimits<float>::Max();
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		const IStrikeInteractable* Interactable = Cast<IStrikeInteractable>(Candidate);
		if (Interactable && Interactable->CanInteract(this))
		{
			const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), GetActorLocation());
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Candidate;
			}
		}
	}
	return Best;
}

// ---------------------------------------------------------------------------
// Dégâts et mort
// ---------------------------------------------------------------------------

float AStrikeCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (!HasAuthority() || IsDead())
	{
		return 0.f;
	}

	const float BaseDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	EStrikeHitZone Zone = EStrikeHitZone::Torso;
	FVector Source = DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation();

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointEvent = static_cast<const FPointDamageEvent&>(DamageEvent);
		Zone = UHealthComponent::GetHitZoneFromHit(PointEvent.HitInfo, this);
		Source = PointEvent.HitInfo.TraceStart;
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		const FRadialDamageEvent& RadialEvent = static_cast<const FRadialDamageEvent&>(DamageEvent);
		Source = RadialEvent.Origin;
	}

	// Le multiplicateur de zone est déjà appliqué par l'arme ; la zone sert ici aux stats et au kill feed.
	return HealthComponent->ApplyDamage(BaseDamage, EventInstigator, DamageCauser, Zone, Source);
}

void AStrikeCharacter::HandleDeath(UHealthComponent* HealthComp, AController* Killer, AActor* DamageCauser, bool bHeadshot)
{
	if (WeaponComponent)
	{
		WeaponComponent->StopFire();
	}
	bIsAiming = false;

	PlayDeathCosmetics();

	if (HasAuthority())
	{
		if (AStrikeGameMode* GameMode = GetWorld()->GetAuthGameMode<AStrikeGameMode>())
		{
			GameMode->NotifyKilled(Killer, GetController(), this, bHeadshot, DamageCauser);
		}
		WeaponComponent->DestroyAllWeapons();
		DetachFromControllerPendingDestroy();
		SetLifeSpan(8.f);
	}
}

void AStrikeCharacter::PlayDeathCosmetics()
{
	if (bDeathCosmeticsPlayed)
	{
		return;
	}
	bDeathCosmeticsPlayed = true;

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();

	USkeletalMeshComponent* SkelMesh = GetMesh();
	if (SkelMesh && SkelMesh->GetSkeletalMeshAsset() && SkelMesh->GetPhysicsAsset())
	{
		// Ragdoll Chaos si un Skeletal Mesh avec Physics Asset est assigné.
		SkelMesh->SetCollisionProfileName(TEXT("Ragdoll"));
		SkelMesh->SetAllBodiesSimulatePhysics(true);
		SkelMesh->WakeAllRigidBodies();
		return;
	}

	// Corps temporaire : chute physique simple (Chaos).
	for (UStaticMeshComponent* Part : { BodyMesh.Get(), HeadMesh.Get() })
	{
		if (Part)
		{
			Part->SetOwnerNoSee(false);
			Part->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
			Part->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
			Part->SetCollisionResponseToChannel(COLLISION_WEAPON, ECR_Ignore);
			Part->SetSimulatePhysics(true);
			Part->AddImpulse(GetActorForwardVector() * -150.f + FVector(0.f, 0.f, 80.f), NAME_None, true);
		}
	}
}

void AStrikeCharacter::Destroyed()
{
	if (HasAuthority() && WeaponComponent)
	{
		WeaponComponent->DestroyAllWeapons();
	}
	Super::Destroyed();
}

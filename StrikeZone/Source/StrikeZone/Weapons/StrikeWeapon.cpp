// STRIKE ZONE - classe de base des armes (hitscan et projectiles, tir validé par le serveur)
#include "Weapons/StrikeWeapon.h"
#include "Weapons/StrikeProjectile.h"
#include "Weapons/StrikeTracer.h"
#include "Character/StrikeCharacter.h"
#include "Components/HealthComponent.h"
#include "Components/InventoryComponent.h"
#include "Game/StrikePlayerState.h"
#include "Game/StrikePlayerController.h"
#include "StrikeZone.h"

#include "Animation/AnimMontage.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Perception/AISense_Hearing.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AStrikeWeapon::AStrikeWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicatingMovement(false);
	bNetUseOwnerRelevancy = true;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Root);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);
	if (CubeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CubeMesh.Object);
	}
	if (ShapeMaterial.Succeeded())
	{
		Mesh->SetMaterial(0, ShapeMaterial.Object);
	}

	Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
	Muzzle->SetupAttachment(Mesh);
	Muzzle->SetRelativeLocation(FVector(55.f, 0.f, 0.f));
	Muzzle->SetUsingAbsoluteScale(true);

	MuzzleLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MuzzleLight"));
	MuzzleLight->SetupAttachment(Muzzle);
	MuzzleLight->SetIntensityUnits(ELightUnits::Candelas);
	MuzzleLight->SetIntensity(0.f);
	MuzzleLight->SetAttenuationRadius(350.f);
	MuzzleLight->SetLightColor(FLinearColor(1.f, 0.65f, 0.3f));
	MuzzleLight->SetCastShadows(false);
}

void AStrikeWeapon::BeginPlay()
{
	Super::BeginPlay();

	Mesh->SetRelativeScale3D(PlaceholderScale);
	Mesh->SetRelativeLocation(HipOffset);
	MeshMaterial = Mesh->CreateDynamicMaterialInstance(0);
	if (MeshMaterial)
	{
		MeshMaterial->SetVectorParameterValue(TEXT("Color"), PlaceholderColor);
	}

	if (HasAuthority())
	{
		CurrentAmmo = MagazineSize;
	}
	RefreshAttachment();
}

void AStrikeWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeWeapon, OwningCharacter);
	DOREPLIFETIME(AStrikeWeapon, CurrentAmmo);
	DOREPLIFETIME_CONDITION(AStrikeWeapon, ServerShotCount, COND_OwnerOnly);
	DOREPLIFETIME(AStrikeWeapon, bIsReloading);
	DOREPLIFETIME(AStrikeWeapon, bIsEquipped);
}

// ---------------------------------------------------------------------------
// Propriétaire / équipement
// ---------------------------------------------------------------------------

void AStrikeWeapon::SetOwningCharacter(AStrikeCharacter* NewOwner)
{
	OwningCharacter = NewOwner;
	SetOwner(NewOwner);
	SetInstigator(NewOwner);
	RefreshAttachment();
}

void AStrikeWeapon::OnRep_OwningCharacter()
{
	RefreshAttachment();
}

void AStrikeWeapon::OnRep_Equipped()
{
	RefreshAttachment();
	if (bIsEquipped)
	{
		EquipReadyTime = GetWorld()->GetTimeSeconds() + EquipTime;
	}
}

void AStrikeWeapon::RefreshAttachment()
{
	if (OwningCharacter && OwningCharacter->GetWeaponAttachPoint())
	{
		AttachToComponent(OwningCharacter->GetWeaponAttachPoint(), FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	SetActorHiddenInGame(!bIsEquipped || !OwningCharacter);
}

void AStrikeWeapon::OnEquipped()
{
	bIsEquipped = true;
	EquipReadyTime = GetWorld()->GetTimeSeconds() + EquipTime;
	RefreshAttachment();
}

void AStrikeWeapon::OnUnequipped()
{
	StopFire();
	if (HasAuthority())
	{
		bIsReloading = false;
		GetWorldTimerManager().ClearTimer(ReloadTimer);
	}
	bIsEquipped = false;
	PendingRecoil = FRotator::ZeroRotator;
	RefreshAttachment();
}

bool AStrikeWeapon::IsLocallyControlledOwner() const
{
	return OwningCharacter && OwningCharacter->IsLocallyControlled();
}

// ---------------------------------------------------------------------------
// Munitions
// ---------------------------------------------------------------------------

int32 AStrikeWeapon::GetCurrentAmmo() const
{
	if (!HasAuthority() && IsLocallyControlledOwner())
	{
		const int32 PendingShots = FMath::Max(0, LocalShotCount - ServerShotCount);
		return FMath::Max(0, CurrentAmmo - PendingShots);
	}
	return CurrentAmmo;
}

int32 AStrikeWeapon::GetReserveAmmo() const
{
	const UInventoryComponent* Inventory = OwningCharacter ? OwningCharacter->GetInventoryComponent() : nullptr;
	return Inventory ? Inventory->GetAmmo(AmmoType) : 0;
}

void AStrikeWeapon::ServerSetMagazineFull()
{
	if (HasAuthority())
	{
		CurrentAmmo = MagazineSize;
	}
}

bool AStrikeWeapon::CanFire() const
{
	return OwningCharacter && !OwningCharacter->IsDead() && bIsEquipped && !bIsReloading
		&& GetCurrentAmmo() > 0 && GetWorld()->GetTimeSeconds() >= EquipReadyTime;
}

bool AStrikeWeapon::CanReload() const
{
	return OwningCharacter && !OwningCharacter->IsDead() && bIsEquipped && !bIsReloading
		&& GetCurrentAmmo() < MagazineSize && GetReserveAmmo() > 0;
}

// ---------------------------------------------------------------------------
// Tir
// ---------------------------------------------------------------------------

void AStrikeWeapon::StartFire()
{
	if (bWantsToFire)
	{
		return;
	}
	bWantsToFire = true;
	bFiredSinceTriggerPull = false;
	HandleFireLoop();
}

void AStrikeWeapon::StopFire()
{
	bWantsToFire = false;
	GetWorldTimerManager().ClearTimer(FireLoopTimer);
}

void AStrikeWeapon::HandleFireLoop()
{
	if (!bWantsToFire || !OwningCharacter)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const float Wait = FMath::Max(LastLocalFireTime + GetFireInterval(), EquipReadyTime) - Now;
	if (Wait > KINDA_SMALL_NUMBER)
	{
		GetWorldTimerManager().SetTimer(FireLoopTimer, this, &AStrikeWeapon::HandleFireLoop, Wait, false);
		return;
	}

	if (!bAutomatic && bFiredSinceTriggerPull)
	{
		return;
	}

	if (GetCurrentAmmo() <= 0)
	{
		StopFire();
		StartReload();
		return;
	}

	if (!CanFire())
	{
		return;
	}

	FireShotLocal();
	LastLocalFireTime = Now;
	bFiredSinceTriggerPull = true;

	if (bAutomatic)
	{
		GetWorldTimerManager().SetTimer(FireLoopTimer, this, &AStrikeWeapon::HandleFireLoop, GetFireInterval(), false);
	}
}

void AStrikeWeapon::GetAimPoint(FVector& OutOrigin, FVector& OutDirection) const
{
	FRotator ViewRotation = FRotator::ZeroRotator;
	OutOrigin = GetActorLocation();
	if (const AController* Controller = OwningCharacter ? OwningCharacter->GetController() : nullptr)
	{
		Controller->GetPlayerViewPoint(OutOrigin, ViewRotation);
	}
	else if (OwningCharacter)
	{
		OwningCharacter->GetActorEyesViewPoint(OutOrigin, ViewRotation);
	}
	OutDirection = ViewRotation.Vector();
}

void AStrikeWeapon::FireShotLocal()
{
	FVector Origin, Direction;
	GetAimPoint(Origin, Direction);

	++LocalShotCount;
	const int32 Seed = LocalShotCount;

	if (HasAuthority())
	{
		// Hôte de partie (listen server) ou IA : traitement direct.
		ServerShotCount = LocalShotCount;
		ProcessShotAuthority(Origin, Direction, Seed);
	}
	else
	{
		// Client : prédiction purement cosmétique puis requête serveur.
		const float Spread = GetCurrentSpread();
		ConsumeSpreadForShot();
		ServerFireShot(Origin, Direction, Seed);

		FRandomStream Stream(Seed);
		const FVector Dir = ApplySpread(Direction, Stream, Spread);
		const FVector End = Origin + Dir * MaxRange;
		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(StrikeWeaponCosmetic), false, OwningCharacter);
		Params.AddIgnoredActor(this);
		const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, COLLISION_WEAPON, Params);
		PlayFireCosmetics(bHit ? Hit.ImpactPoint : End);
		if (bHit && FireType == EStrikeFireType::Hitscan)
		{
			PlayImpactCosmetics(Hit.ImpactPoint, Hit.ImpactNormal);
		}
	}

	// Recul appliqué à la caméra du joueur local.
	if (OwningCharacter && OwningCharacter->IsPlayerControlled())
	{
		const float AimFactor = FMath::Lerp(1.f, 0.65f, OwningCharacter->GetAimAlpha());
		const float CrouchFactor = OwningCharacter->bIsCrouched ? 0.8f : 1.f;
		PendingRecoil.Pitch += FMath::FRandRange(RecoilPitchMin, RecoilPitchMax) * AimFactor * CrouchFactor;
		PendingRecoil.Yaw += FMath::FRandRange(-RecoilYaw, RecoilYaw) * AimFactor;
	}
}

bool AStrikeWeapon::ServerFireShot_Validate(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, int32 Seed)
{
	return !Origin.ContainsNaN() && !Direction.ContainsNaN();
}

void AStrikeWeapon::ServerFireShot_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Direction, int32 Seed)
{
	++ServerShotCount;

	if (!OwningCharacter || OwningCharacter->IsDead() || !bIsEquipped || bIsReloading || CurrentAmmo <= 0)
	{
		return;
	}

	// Cadence : seau de jetons, tolère la gigue réseau mais interdit une cadence soutenue trop élevée.
	const float Now = GetWorld()->GetTimeSeconds();
	const float Interval = GetFireInterval();
	ServerFireCredits = FMath::Min(2.f, ServerFireCredits + (Now - ServerLastCreditTime) / Interval);
	ServerLastCreditTime = Now;
	if (ServerFireCredits < 0.9f)
	{
		UE_LOG(LogStrike, Warning, TEXT("Tir rejeté (cadence) pour %s"), *GetNameSafe(OwningCharacter));
		return;
	}
	ServerFireCredits -= 1.f;

	// Origine : doit être proche de l'œil connu du serveur.
	FVector ValidOrigin = Origin;
	const FVector ServerEye = OwningCharacter->GetPawnViewLocation();
	const float Tolerance = 150.f + OwningCharacter->GetVelocity().Size() * 0.25f;
	if (FVector::DistSquared(ServerEye, ValidOrigin) > FMath::Square(Tolerance))
	{
		UE_LOG(LogStrike, Warning, TEXT("Origine de tir corrigée pour %s"), *GetNameSafe(OwningCharacter));
		ValidOrigin = ServerEye;
	}

	// Direction : doit être cohérente avec la rotation de contrôle côté serveur.
	const FVector ServerAim = OwningCharacter->GetBaseAimRotation().Vector();
	FVector ValidDirection = FVector(Direction).GetSafeNormal();
	if (ValidDirection.IsNearlyZero() || FVector::DotProduct(ValidDirection, ServerAim) < FMath::Cos(FMath::DegreesToRadians(40.f)))
	{
		ValidDirection = ServerAim;
	}

	ProcessShotAuthority(ValidOrigin, ValidDirection, Seed);
}

void AStrikeWeapon::ProcessShotAuthority(const FVector& Origin, const FVector& AimDirection, int32 Seed)
{
	AStrikeCharacter* Shooter = OwningCharacter;
	UWorld* World = GetWorld();
	if (!Shooter || !World)
	{
		return;
	}

	CurrentAmmo = FMath::Max(0, CurrentAmmo - 1);

	AController* ShooterController = Shooter->GetController();
	AStrikePlayerState* ShooterPS = ShooterController ? ShooterController->GetPlayerState<AStrikePlayerState>() : nullptr;
	if (ShooterPS)
	{
		ShooterPS->AddShotFired();
	}

	const float Spread = GetCurrentSpread();
	ConsumeSpreadForShot();
	FRandomStream Stream(Seed);

	if (FireType == EStrikeFireType::Projectile && ProjectileClass)
	{
		const FVector Dir = ApplySpread(AimDirection, Stream, Spread);
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = Shooter;
		SpawnParams.Instigator = Shooter;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AStrikeProjectile* Projectile = World->SpawnActor<AStrikeProjectile>(ProjectileClass, Origin + Dir * 80.f, Dir.Rotation(), SpawnParams))
		{
			Projectile->InitProjectile(Damage, WeaponName);
		}
		MulticastFireFX(Origin + Dir * 1000.f, -Dir, false);
	}
	else
	{
		struct FPendingDamage
		{
			float Amount = 0.f;
			FHitResult Hit;
			FVector Direction = FVector::ForwardVector;
			bool bHeadshot = false;
		};
		TMap<AActor*, FPendingDamage> DamageByActor;

		FCollisionQueryParams Params(SCENE_QUERY_STAT(StrikeWeaponTrace), false, Shooter);
		Params.AddIgnoredActor(this);

		FVector FirstEnd = Origin + AimDirection * MaxRange;
		FVector FirstNormal = -AimDirection;
		bool bFirstHit = false;

		for (int32 Pellet = 0; Pellet < FMath::Max(1, PelletsPerShot); ++Pellet)
		{
			const FVector Dir = ApplySpread(AimDirection, Stream, Spread);
			const FVector End = Origin + Dir * MaxRange;

			FHitResult Hit;
			const bool bHit = World->LineTraceSingleByChannel(Hit, Origin, End, COLLISION_WEAPON, Params);
			if (Pellet == 0)
			{
				FirstEnd = bHit ? Hit.ImpactPoint : End;
				FirstNormal = bHit ? FVector(Hit.ImpactNormal) : -Dir;
				bFirstHit = bHit;
			}
			if (!bHit || !Hit.GetActor() || !Hit.GetActor()->CanBeDamaged())
			{
				continue;
			}

			AActor* HitActor = Hit.GetActor();
			const EStrikeHitZone Zone = UHealthComponent::GetHitZoneFromHit(Hit, HitActor);
			const float PelletDamage = Damage * GetDamageFalloff(Hit.Distance) * ZoneMultipliers.Get(Zone);

			FPendingDamage& Pending = DamageByActor.FindOrAdd(HitActor);
			Pending.Amount += PelletDamage;
			Pending.Direction = Dir;
			if (!Pending.bHeadshot || Zone == EStrikeHitZone::Head)
			{
				Pending.Hit = Hit;
			}
			Pending.bHeadshot |= (Zone == EStrikeHitZone::Head);
		}

		bool bAnyHit = false;
		bool bHeadshot = false;
		bool bKill = false;
		for (TPair<AActor*, FPendingDamage>& Pair : DamageByActor)
		{
			if (!IsValid(Pair.Key))
			{
				continue;
			}
			const UHealthComponent* VictimHealth = Pair.Key->FindComponentByClass<UHealthComponent>();
			const bool bWasAlive = !VictimHealth || !VictimHealth->IsDead();

			const float Dealt = UGameplayStatics::ApplyPointDamage(Pair.Key, Pair.Value.Amount, Pair.Value.Direction, Pair.Value.Hit, ShooterController, this, UDamageType::StaticClass());
			if (Dealt > 0.f)
			{
				bAnyHit = true;
				bHeadshot |= Pair.Value.bHeadshot;
				bKill |= bWasAlive && VictimHealth && VictimHealth->IsDead();
			}
		}

		if (bAnyHit)
		{
			if (ShooterPS)
			{
				ShooterPS->AddShotHit(bHeadshot);
			}
			if (Shooter->IsPlayerControlled())
			{
				ClientConfirmHit(bHeadshot, bKill);
			}
		}

		MulticastFireFX(FirstEnd, FirstNormal, bFirstHit);
	}

	UAISense_Hearing::ReportNoiseEvent(this, Origin, 1.f, Shooter, NoiseRange, TEXT("Gunfire"));
}

void AStrikeWeapon::ConsumeSpreadForShot()
{
	SpreadBonus = FMath::Min(MaxSpread, SpreadBonus + SpreadPerShot);
}

FVector AStrikeWeapon::ApplySpread(const FVector& Direction, FRandomStream& Stream, float SpreadDegrees) const
{
	if (SpreadDegrees <= KINDA_SMALL_NUMBER)
	{
		return Direction;
	}
	return Stream.VRandCone(Direction, FMath::DegreesToRadians(SpreadDegrees));
}

float AStrikeWeapon::GetDamageFalloff(float Distance) const
{
	if (Distance <= FalloffStart || FalloffEnd <= FalloffStart)
	{
		return 1.f;
	}
	const float Alpha = FMath::Clamp((Distance - FalloffStart) / (FalloffEnd - FalloffStart), 0.f, 1.f);
	return FMath::Lerp(1.f, MinDamageFactor, Alpha);
}

float AStrikeWeapon::GetCurrentSpread() const
{
	const AStrikeCharacter* Character = OwningCharacter;
	if (!Character)
	{
		return HipSpread;
	}

	float Base = FMath::Lerp(HipSpread, AimSpread, Character->GetAimAlpha());
	float Multiplier = 1.f;

	if (const UCharacterMovementComponent* Move = Character->GetCharacterMovement())
	{
		if (Move->IsFalling())
		{
			Multiplier *= AirSpreadMultiplier;
		}
		else
		{
			const float SpeedAlpha = FMath::Clamp(Character->GetVelocity().Size2D() / 600.f, 0.f, 1.f);
			Multiplier *= FMath::Lerp(1.f, MovingSpreadMultiplier, SpeedAlpha);
			if (Character->bIsCrouched)
			{
				Multiplier *= CrouchSpreadMultiplier;
			}
		}
	}

	const float BonusScale = FMath::Lerp(1.f, 0.5f, Character->GetAimAlpha());
	return FMath::Min(MaxSpread, (Base + SpreadBonus * BonusScale) * Multiplier);
}

// ---------------------------------------------------------------------------
// Rechargement
// ---------------------------------------------------------------------------

void AStrikeWeapon::StartReload()
{
	if (!CanReload())
	{
		return;
	}

	StopFire();

	if (HasAuthority())
	{
		ServerStartReload_Implementation();
	}
	else
	{
		bIsReloading = true; // prédiction locale
		ServerStartReload();
		// Filet de sécurité si le serveur refuse : on lève la prédiction après la durée prévue.
		FTimerHandle ResetHandle;
		GetWorldTimerManager().SetTimer(ResetHandle, FTimerDelegate::CreateWeakLambda(this, [this]() { bIsReloading = false; }), ReloadTime + 0.35f, false);
		OnRep_Reloading();
	}
}

void AStrikeWeapon::ServerStartReload_Implementation()
{
	if (!OwningCharacter || OwningCharacter->IsDead() || !bIsEquipped || bIsReloading
		|| CurrentAmmo >= MagazineSize || GetReserveAmmo() <= 0)
	{
		return;
	}
	bIsReloading = true;
	GetWorldTimerManager().SetTimer(ReloadTimer, this, &AStrikeWeapon::FinishReload, ReloadTime, false);
	OnRep_Reloading();
}

void AStrikeWeapon::FinishReload()
{
	if (!HasAuthority() || !OwningCharacter)
	{
		return;
	}
	bIsReloading = false;
	if (UInventoryComponent* Inventory = OwningCharacter->GetInventoryComponent())
	{
		const int32 Needed = MagazineSize - CurrentAmmo;
		CurrentAmmo += Inventory->ConsumeAmmo(AmmoType, Needed);
	}
}

void AStrikeWeapon::OnRep_Reloading()
{
	if (!bIsReloading || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	}
	if (ReloadMontage && OwningCharacter)
	{
		OwningCharacter->PlayAnimMontage(ReloadMontage);
	}
}

// ---------------------------------------------------------------------------
// Cosmétique
// ---------------------------------------------------------------------------

void AStrikeWeapon::MulticastFireFX_Implementation(FVector_NetQuantize TraceEnd, FVector_NetQuantizeNormal ImpactNormal, bool bHit)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// Le client tireur a déjà joué ses effets en prédiction.
	if (!HasAuthority() && IsLocallyControlledOwner())
	{
		return;
	}
	PlayFireCosmetics(TraceEnd);
	if (bHit && FireType == EStrikeFireType::Hitscan)
	{
		PlayImpactCosmetics(TraceEnd, ImpactNormal);
	}
}

void AStrikeWeapon::PlayFireCosmetics(const FVector& TraceEnd)
{
	if (GetNetMode() == NM_DedicatedServer)
	{
		return;
	}

	MuzzleFlashRemaining = 0.05f;
	KickBack = 1.f;

	const FVector MuzzleLocation = Muzzle->GetComponentLocation();
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, MuzzleLocation);
	}
	if (MuzzleFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleFX, Muzzle, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::SnapToTarget, true);
	}
	if (FireMontage && OwningCharacter)
	{
		OwningCharacter->PlayAnimMontage(FireMontage);
	}
	if (bSpawnTracers && FireType == EStrikeFireType::Hitscan)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AStrikeTracer* Tracer = GetWorld()->SpawnActor<AStrikeTracer>(AStrikeTracer::StaticClass(), MuzzleLocation, FRotator::ZeroRotator, Params))
		{
			Tracer->InitTracer(MuzzleLocation, TraceEnd);
		}
	}
}

void AStrikeWeapon::PlayImpactCosmetics(const FVector& Location, const FVector& Normal)
{
	if (ImpactFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactFX, Location, Normal.Rotation());
	}
}

void AStrikeWeapon::ClientConfirmHit_Implementation(bool bHeadshot, bool bKill)
{
	if (OwningCharacter)
	{
		if (AStrikePlayerController* PC = Cast<AStrikePlayerController>(OwningCharacter->GetController()))
		{
			PC->NotifyHitConfirmed(bHeadshot, bKill);
		}
	}
}

void AStrikeWeapon::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Récupération de la dispersion (client et serveur).
	if (GetWorld()->GetTimeSeconds() - LastLocalFireTime > GetFireInterval() * 1.2f || !bWantsToFire)
	{
		SpreadBonus = FMath::Max(0.f, SpreadBonus - SpreadRecovery * DeltaSeconds);
	}

	if (GetNetMode() != NM_DedicatedServer)
	{
		UpdateWeaponPose(DeltaSeconds);
		MuzzleFlashRemaining -= DeltaSeconds;
		MuzzleLight->SetIntensity(MuzzleFlashRemaining > 0.f ? 600.f : 0.f);
	}

	if (IsLocallyControlledOwner())
	{
		ApplyRecoilToController(DeltaSeconds);
	}
}

void AStrikeWeapon::UpdateWeaponPose(float DeltaSeconds)
{
	if (!OwningCharacter || !bIsEquipped)
	{
		return;
	}

	// Animation procédurale temporaire (remplacée par l'Animation Blueprint quand les assets existent).
	const float Aim = OwningCharacter->GetAimAlpha();
	KickBack = FMath::FInterpTo(KickBack, 0.f, DeltaSeconds, 18.f);

	FVector Offset = FMath::Lerp(HipOffset, AimOffset, Aim);
	Offset.X -= KickBack * FMath::Lerp(5.f, 2.f, Aim);

	const float Speed = OwningCharacter->GetVelocity().Size2D();
	if (Speed > 10.f && OwningCharacter->GetCharacterMovement()->IsMovingOnGround())
	{
		const float Time = GetWorld()->GetTimeSeconds();
		const float BobScale = FMath::Clamp(Speed / 450.f, 0.f, 1.6f) * FMath::Lerp(1.f, 0.15f, Aim);
		Offset.Z += FMath::Sin(Time * 11.f) * 0.8f * BobScale;
		Offset.Y += FMath::Cos(Time * 5.5f) * 0.6f * BobScale;
	}

	FRotator TargetRot = FRotator::ZeroRotator;
	if (bIsReloading)
	{
		TargetRot = FRotator(-30.f, 0.f, 25.f);
	}
	else if (OwningCharacter->IsSprinting())
	{
		TargetRot = FRotator(-12.f, 28.f, 0.f);
	}
	TargetRot.Pitch += KickBack * 3.f;

	const FRotator NewRot = FMath::RInterpTo(Mesh->GetRelativeRotation(), TargetRot, DeltaSeconds, 10.f);
	Mesh->SetRelativeLocationAndRotation(Offset, NewRot);
}

void AStrikeWeapon::ApplyRecoilToController(float DeltaSeconds)
{
	APlayerController* PC = OwningCharacter ? Cast<APlayerController>(OwningCharacter->GetController()) : nullptr;
	if (!PC)
	{
		return;
	}

	FRotator Delta = FRotator::ZeroRotator;
	if (!PendingRecoil.IsNearlyZero(0.001f))
	{
		const float Alpha = FMath::Min(1.f, DeltaSeconds * 30.f);
		Delta = PendingRecoil * Alpha;
		PendingRecoil -= Delta;
		AccumulatedRecoilPitch += Delta.Pitch * 0.6f;
	}
	else if (!bWantsToFire && AccumulatedRecoilPitch > 0.01f && RecoilRecoverySpeed > 0.f)
	{
		const float Recover = AccumulatedRecoilPitch * FMath::Min(1.f, DeltaSeconds * RecoilRecoverySpeed);
		AccumulatedRecoilPitch -= Recover;
		Delta.Pitch = -Recover;
	}

	if (!Delta.IsNearlyZero(0.0001f))
	{
		FRotator Control = PC->GetControlRotation();
		Control.Pitch = FMath::ClampAngle(Control.Pitch + Delta.Pitch, -89.f, 89.f);
		Control.Yaw += Delta.Yaw;
		PC->SetControlRotation(Control);
	}
}

// STRIKE ZONE - armes concrètes (statistiques réglables en Blueprint enfant)
#include "Weapons/StrikeWeaponTypes.h"
#include "GameFramework/ProjectileMovementComponent.h"

AStrikeWeapon_Pistol::AStrikeWeapon_Pistol()
{
	WeaponName = TEXT("P-9 Pistolet");
	AmmoType = EStrikeAmmoType::Pistol;
	MagazineSize = 12;
	StartingReserveMagazines = 4;
	Damage = 28.f;
	FireRateRPM = 400.f;
	bAutomatic = false;
	FalloffStart = 1500.f;
	FalloffEnd = 4000.f;
	HipSpread = 1.4f;
	AimSpread = 0.2f;
	SpreadPerShot = 0.6f;
	MaxSpread = 4.f;
	RecoilPitchMin = 0.8f;
	RecoilPitchMax = 1.2f;
	RecoilYaw = 0.3f;
	AimFOV = 75.f;
	AimSpeed = 8.f;
	ReloadTime = 1.5f;
	EquipTime = 0.3f;
	BuyPrice = 0;
	PlaceholderScale = FVector(0.28f, 0.06f, 0.12f);
	PlaceholderColor = FLinearColor(0.15f, 0.15f, 0.17f);
	HipOffset = FVector(32.f, 14.f, -15.f);
	AimOffset = FVector(26.f, 0.f, -7.f);
}

AStrikeWeapon_AssaultRifle::AStrikeWeapon_AssaultRifle()
{
	WeaponName = TEXT("AR-56 Fusil d'assaut");
	AmmoType = EStrikeAmmoType::Rifle;
	MagazineSize = 30;
	StartingReserveMagazines = 4;
	Damage = 24.f;
	FireRateRPM = 650.f;
	bAutomatic = true;
	FalloffStart = 3000.f;
	FalloffEnd = 7000.f;
	HipSpread = 2.2f;
	AimSpread = 0.25f;
	SpreadPerShot = 0.3f;
	MaxSpread = 5.f;
	RecoilPitchMin = 0.35f;
	RecoilPitchMax = 0.7f;
	RecoilYaw = 0.22f;
	AimFOV = 62.f;
	AimSpeed = 6.f;
	ReloadTime = 2.1f;
	BuyPrice = 1200;
	PlaceholderScale = FVector(0.7f, 0.07f, 0.12f);
	PlaceholderColor = FLinearColor(0.22f, 0.2f, 0.15f);
}

AStrikeWeapon_SMG::AStrikeWeapon_SMG()
{
	WeaponName = TEXT("V-45 PM");
	AmmoType = EStrikeAmmoType::Pistol;
	MagazineSize = 32;
	StartingReserveMagazines = 4;
	Damage = 19.f;
	FireRateRPM = 900.f;
	bAutomatic = true;
	FalloffStart = 1200.f;
	FalloffEnd = 3500.f;
	MinDamageFactor = 0.5f;
	HipSpread = 1.8f;
	AimSpread = 0.45f;
	SpreadPerShot = 0.22f;
	MaxSpread = 4.5f;
	MovingSpreadMultiplier = 1.3f;
	RecoilPitchMin = 0.25f;
	RecoilPitchMax = 0.45f;
	RecoilYaw = 0.3f;
	AimFOV = 70.f;
	AimSpeed = 8.f;
	ReloadTime = 1.8f;
	BuyPrice = 750;
	PlaceholderScale = FVector(0.5f, 0.07f, 0.13f);
	PlaceholderColor = FLinearColor(0.1f, 0.12f, 0.2f);
}

AStrikeWeapon_Shotgun::AStrikeWeapon_Shotgun()
{
	WeaponName = TEXT("SG-12 Fusil à pompe");
	AmmoType = EStrikeAmmoType::Shotgun;
	MagazineSize = 6;
	StartingReserveMagazines = 4;
	Damage = 16.f;
	PelletsPerShot = 8;
	FireRateRPM = 75.f;
	bAutomatic = false;
	MaxRange = 4000.f;
	FalloffStart = 500.f;
	FalloffEnd = 1800.f;
	MinDamageFactor = 0.2f;
	ZoneMultipliers.Head = 1.5f;
	HipSpread = 5.5f;
	AimSpread = 4.f;
	SpreadPerShot = 0.f;
	MaxSpread = 6.f;
	MovingSpreadMultiplier = 1.1f;
	AirSpreadMultiplier = 1.2f;
	RecoilPitchMin = 3.f;
	RecoilPitchMax = 4.f;
	RecoilYaw = 0.8f;
	AimFOV = 75.f;
	ReloadTime = 2.8f;
	BuyPrice = 1000;
	PlaceholderScale = FVector(0.75f, 0.09f, 0.11f);
	PlaceholderColor = FLinearColor(0.3f, 0.18f, 0.08f);
}

AStrikeWeapon_Sniper::AStrikeWeapon_Sniper()
{
	WeaponName = TEXT("LR-338 Précision");
	AmmoType = EStrikeAmmoType::Sniper;
	MagazineSize = 5;
	StartingReserveMagazines = 4;
	Damage = 90.f;
	FireRateRPM = 45.f;
	bAutomatic = false;
	MaxRange = 30000.f;
	FalloffStart = 20000.f;
	FalloffEnd = 30000.f;
	MinDamageFactor = 0.85f;
	ZoneMultipliers.Head = 2.5f;
	ZoneMultipliers.Legs = 0.8f;
	HipSpread = 6.f;
	AimSpread = 0.f;
	SpreadPerShot = 2.f;
	MaxSpread = 8.f;
	RecoilPitchMin = 4.f;
	RecoilPitchMax = 5.f;
	RecoilYaw = 0.5f;
	RecoilRecoverySpeed = 3.f;
	AimFOV = 25.f;
	AimSpeed = 4.f;
	ReloadTime = 3.f;
	EquipTime = 0.6f;
	BuyPrice = 1500;
	NoiseRange = 8000.f;
	PlaceholderScale = FVector(0.95f, 0.06f, 0.11f);
	PlaceholderColor = FLinearColor(0.12f, 0.18f, 0.12f);
}

AStrikeProjectile_Rocket::AStrikeProjectile_Rocket()
{
	DirectDamage = 60.f;
	ExplosionRadius = 450.f;
	ExplosionDamage = 140.f;
	Movement->InitialSpeed = 3500.f;
	Movement->MaxSpeed = 3500.f;
	Movement->ProjectileGravityScale = 0.f;
	InitialLifeSpan = 8.f;
}

AStrikeWeapon_RocketLauncher::AStrikeWeapon_RocketLauncher()
{
	WeaponName = TEXT("RL-7 Lance-roquettes");
	FireType = EStrikeFireType::Projectile;
	ProjectileClass = AStrikeProjectile_Rocket::StaticClass();
	AmmoType = EStrikeAmmoType::Rocket;
	MagazineSize = 1;
	StartingReserveMagazines = 3;
	Damage = 60.f;
	FireRateRPM = 50.f;
	bAutomatic = false;
	HipSpread = 1.f;
	AimSpread = 0.f;
	SpreadPerShot = 0.f;
	RecoilPitchMin = 3.f;
	RecoilPitchMax = 4.f;
	AimFOV = 60.f;
	AimSpeed = 4.f;
	ReloadTime = 3.f;
	EquipTime = 0.8f;
	BuyPrice = 3000;
	NoiseRange = 6000.f;
	PlaceholderScale = FVector(0.9f, 0.16f, 0.16f);
	PlaceholderColor = FLinearColor(0.2f, 0.25f, 0.1f);
	HipOffset = FVector(30.f, 18.f, -12.f);
	AimOffset = FVector(24.f, 6.f, -4.f);
}

// STRIKE ZONE - armes concrètes (statistiques réglables en Blueprint enfant)
#pragma once

#include "CoreMinimal.h"
#include "Weapons/StrikeWeapon.h"
#include "Weapons/StrikeProjectile.h"
#include "StrikeWeaponTypes.generated.h"

/** Pistolet semi-automatique (arme de départ). */
UCLASS()
class STRIKEZONE_API AStrikeWeapon_Pistol : public AStrikeWeapon
{
	GENERATED_BODY()
public:
	AStrikeWeapon_Pistol();
};

/** Fusil d'assaut automatique polyvalent. */
UCLASS()
class STRIKEZONE_API AStrikeWeapon_AssaultRifle : public AStrikeWeapon
{
	GENERATED_BODY()
public:
	AStrikeWeapon_AssaultRifle();
};

/** Pistolet-mitrailleur : cadence élevée, courte portée. */
UCLASS()
class STRIKEZONE_API AStrikeWeapon_SMG : public AStrikeWeapon
{
	GENERATED_BODY()
public:
	AStrikeWeapon_SMG();
};

/** Fusil à pompe : 8 plombs, gros dégâts à courte distance. */
UCLASS()
class STRIKEZONE_API AStrikeWeapon_Shotgun : public AStrikeWeapon
{
	GENERATED_BODY()
public:
	AStrikeWeapon_Shotgun();
};

/** Fusil de précision : un coup à la tête est mortel. */
UCLASS()
class STRIKEZONE_API AStrikeWeapon_Sniper : public AStrikeWeapon
{
	GENERATED_BODY()
public:
	AStrikeWeapon_Sniper();
};

/** Roquette explosive. */
UCLASS()
class STRIKEZONE_API AStrikeProjectile_Rocket : public AStrikeProjectile
{
	GENERATED_BODY()
public:
	AStrikeProjectile_Rocket();
};

/** Lance-roquettes (projectile physique). */
UCLASS()
class STRIKEZONE_API AStrikeWeapon_RocketLauncher : public AStrikeWeapon
{
	GENERATED_BODY()
public:
	AStrikeWeapon_RocketLauncher();
};

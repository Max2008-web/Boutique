// STRIKE ZONE - types partagés par tous les systèmes
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "StrikeTypes.generated.h"

class AStrikeCharacter;

/** Canal de trace "Weapon" déclaré dans DefaultEngine.ini (ECC_GameTraceChannel1). */
#define COLLISION_WEAPON ECC_GameTraceChannel1

namespace StrikeTeams
{
	/** Aucune équipe (spectateur, avant affectation). */
	constexpr uint8 NoTeam = 255;
	/** Équipe réservée aux zombies. */
	constexpr uint8 Zombies = 254;
	/** Ennemis IA du mode Extraction. */
	constexpr uint8 Hostiles = 253;
}

UENUM(BlueprintType)
enum class EStrikeHitZone : uint8
{
	Head,
	Torso,
	Arms,
	Legs
};

UENUM(BlueprintType)
enum class EStrikeAmmoType : uint8
{
	Pistol,
	Rifle,
	Shotgun,
	Sniper,
	Rocket,
	MAX UMETA(Hidden)
};

UENUM(BlueprintType)
enum class EStrikeFireType : uint8
{
	Hitscan,
	Projectile
};

UENUM(BlueprintType)
enum class EStrikeAIDifficulty : uint8
{
	Easy,
	Normal,
	Hard,
	Veteran
};

UENUM(BlueprintType)
enum class EStrikeModeType : uint8
{
	Menu,
	Zombies,
	QuickMatch,
	BattleRoyale,
	Tactical,
	Extraction,
	Training
};

UENUM(BlueprintType)
enum class EStrikeArenaLayout : uint8
{
	/** Arène fermée ~60 m (Zombies, Partie rapide, Tactique, Menu). */
	Arena,
	/** Grande carte ~300 m (Battle Royale, Extraction). */
	Large,
	/** Stand de tir (Entraînement). */
	Range
};

/** Multiplicateurs de dégâts localisés. */
USTRUCT(BlueprintType)
struct FStrikeZoneMultipliers
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float Head = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float Torso = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float Arms = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	float Legs = 0.75f;

	float Get(EStrikeHitZone Zone) const
	{
		switch (Zone)
		{
		case EStrikeHitZone::Head: return Head;
		case EStrikeHitZone::Arms: return Arms;
		case EStrikeHitZone::Legs: return Legs;
		default: return Torso;
		}
	}
};

USTRUCT(BlueprintType)
struct FStrikeAmmoStack
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	EStrikeAmmoType AmmoType = EStrikeAmmoType::Rifle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ammo")
	int32 Count = 0;
};

/** Objet de butin (mode Extraction, inventaire persistant). */
USTRUCT(BlueprintType)
struct FStrikeLootItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loot")
	int32 Value = 0;
};

/** Interface des objets avec lesquels un joueur peut interagir (achat, ramassage, objectifs). */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UStrikeInteractable : public UInterface
{
	GENERATED_BODY()
};

class STRIKEZONE_API IStrikeInteractable
{
	GENERATED_BODY()

public:
	/** Appelé sur le client (affichage) et sur le serveur (validation). */
	virtual bool CanInteract(const AStrikeCharacter* User) const { return true; }

	/** Exécuté uniquement sur le serveur. */
	virtual void Interact(AStrikeCharacter* User) {}

	/** Texte affiché par le HUD. */
	virtual FString GetInteractPrompt(const AStrikeCharacter* User) const { return TEXT("Utiliser"); }
};

namespace StrikeUtils
{
	inline FString AmmoTypeToString(EStrikeAmmoType Type)
	{
		switch (Type)
		{
		case EStrikeAmmoType::Pistol: return TEXT("9 mm");
		case EStrikeAmmoType::Rifle: return TEXT("5.56");
		case EStrikeAmmoType::Shotgun: return TEXT("Cal. 12");
		case EStrikeAmmoType::Sniper: return TEXT(".338");
		case EStrikeAmmoType::Rocket: return TEXT("Roquette");
		default: return TEXT("?");
		}
	}

	inline FLinearColor TeamColor(uint8 TeamId)
	{
		switch (TeamId)
		{
		case 0: return FLinearColor(0.10f, 0.35f, 0.95f);
		case 1: return FLinearColor(0.95f, 0.25f, 0.10f);
		case StrikeTeams::Zombies: return FLinearColor(0.25f, 0.55f, 0.15f);
		case StrikeTeams::Hostiles: return FLinearColor(0.35f, 0.30f, 0.20f);
		default:
		{
			// Couleur pseudo-aléatoire stable pour le Battle Royale (une équipe par joueur).
			const float Hue = static_cast<float>((TeamId * 47) % 360);
			return FLinearColor::MakeFromHSV8(static_cast<uint8>(Hue / 360.f * 255.f), 180, 230);
		}
		}
	}
}

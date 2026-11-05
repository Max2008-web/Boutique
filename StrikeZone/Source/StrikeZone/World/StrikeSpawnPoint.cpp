// STRIKE ZONE - point d'apparition (joueurs par équipe, zombies, ennemis)
#include "World/StrikeSpawnPoint.h"
#include "Components/ArrowComponent.h"
#include "Components/CapsuleComponent.h"

AStrikeSpawnPoint::AStrikeSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(38.f, 88.f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Capsule->SetHiddenInGame(true);
	RootComponent = Capsule;

#if WITH_EDITORONLY_DATA
	Arrow = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("Arrow"));
	if (Arrow)
	{
		Arrow->SetupAttachment(Capsule);
	}
#endif
}

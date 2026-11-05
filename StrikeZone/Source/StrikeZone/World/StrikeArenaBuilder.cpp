// STRIKE ZONE - construction procédurale des cartes (aucun asset requis)
#include "World/StrikeArenaBuilder.h"
#include "World/StrikeSpawnPoint.h"
#include "StrikeZone.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	UInstancedStaticMeshComponent* MakeBlockComponent(AActor* Owner, const TCHAR* Name, UStaticMesh* Mesh, UMaterialInterface* Material, USceneComponent* Parent)
	{
		UInstancedStaticMeshComponent* Comp = Owner->CreateDefaultSubobject<UInstancedStaticMeshComponent>(Name);
		Comp->SetupAttachment(Parent);
		Comp->SetMobility(EComponentMobility::Movable); // instances ajoutées à l'exécution
		Comp->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Comp->SetCanEverAffectNavigation(true);
		if (Mesh)
		{
			Comp->SetStaticMesh(Mesh);
		}
		if (Material)
		{
			Comp->SetMaterial(0, Material);
		}
		return Comp;
	}
}

AStrikeArenaBuilder::AStrikeArenaBuilder()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicatingMovement(false);

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SceneRoot->SetMobility(EComponentMobility::Movable);
	RootComponent = SceneRoot;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	UStaticMesh* Mesh = CubeMesh.Succeeded() ? CubeMesh.Object : nullptr;
	UMaterialInterface* Material = ShapeMaterial.Succeeded() ? ShapeMaterial.Object : nullptr;

	FloorBlocks = MakeBlockComponent(this, TEXT("FloorBlocks"), Mesh, Material, SceneRoot);
	WallBlocks = MakeBlockComponent(this, TEXT("WallBlocks"), Mesh, Material, SceneRoot);
	CoverBlocks = MakeBlockComponent(this, TEXT("CoverBlocks"), Mesh, Material, SceneRoot);
	AccentBlocks = MakeBlockComponent(this, TEXT("AccentBlocks"), Mesh, Material, SceneRoot);
}

void AStrikeArenaBuilder::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrikeArenaBuilder, Layout);
	DOREPLIFETIME(AStrikeArenaBuilder, Seed);
}

void AStrikeArenaBuilder::BeginPlay()
{
	Super::BeginPlay();
	BuildArena();
}

float AStrikeArenaBuilder::GetHalfSize() const
{
	switch (Layout)
	{
	case EStrikeArenaLayout::Large: return 15000.f;
	case EStrikeArenaLayout::Range: return 4000.f;
	default: return 3000.f;
	}
}

void AStrikeArenaBuilder::AddBlock(UInstancedStaticMeshComponent* Target, const FVector& Center, const FVector& Size, const FRotator& Rotation, bool bIsObstacle)
{
	// Le cube moteur mesure 100 cm : l'échelle vaut Taille / 100.
	Target->AddInstance(FTransform(Rotation, Center, Size / 100.f));
	if (bIsObstacle)
	{
		Obstacles.Add(FBox::BuildAABB(Center, Size * 0.5f));
	}
}

bool AStrikeArenaBuilder::IsPointBlocked(const FVector& Point, float Radius) const
{
	for (const FBox& Box : Obstacles)
	{
		if (Box.ExpandBy(Radius).IsInsideXY(Point) && Point.Z < Box.Max.Z + 50.f)
		{
			return true;
		}
	}
	return false;
}

FVector AStrikeArenaBuilder::GetRandomOpenPoint(FRandomStream& Stream, float Margin) const
{
	const float Half = GetHalfSize() - Margin;
	for (int32 Attempt = 0; Attempt < 60; ++Attempt)
	{
		const FVector Candidate(Stream.FRandRange(-Half, Half), Stream.FRandRange(-Half, Half), 100.f);
		if (!IsPointBlocked(Candidate, 120.f))
		{
			return Candidate;
		}
	}
	return FVector(0.f, 0.f, 100.f);
}

void AStrikeArenaBuilder::BuildArena()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;

	FRandomStream Stream(Seed);
	switch (Layout)
	{
	case EStrikeArenaLayout::Large: BuildLargeLayout(Stream); break;
	case EStrikeArenaLayout::Range: BuildRangeLayout(Stream); break;
	default: BuildArenaLayout(Stream); break;
	}

	const auto Tint = [](UInstancedStaticMeshComponent* Comp, const FLinearColor& Color)
	{
		if (UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0))
		{
			MID->SetVectorParameterValue(TEXT("Color"), Color);
		}
	};
	Tint(FloorBlocks, FLinearColor(0.18f, 0.19f, 0.2f));
	Tint(WallBlocks, FLinearColor(0.45f, 0.45f, 0.47f));
	Tint(CoverBlocks, FLinearColor(0.55f, 0.42f, 0.25f));
	Tint(AccentBlocks, FLinearColor(0.85f, 0.6f, 0.1f));

	SpawnLightingIfNeeded();

	if (HasAuthority())
	{
		SpawnServerMarkers();
	}
}

// ---------------------------------------------------------------------------
// Arène 60 m (Zombies, Partie rapide, Tactique, Menu)
// ---------------------------------------------------------------------------

void AStrikeArenaBuilder::BuildArenaLayout(FRandomStream& Stream)
{
	const float Half = 3000.f;
	const float WallH = 600.f;

	AddBlock(FloorBlocks, FVector(0.f, 0.f, -50.f), FVector(Half * 2.f + 400.f, Half * 2.f + 400.f, 100.f), FRotator::ZeroRotator, false);
	AddBlock(WallBlocks, FVector(Half + 50.f, 0.f, WallH * 0.5f), FVector(100.f, Half * 2.f + 200.f, WallH));
	AddBlock(WallBlocks, FVector(-Half - 50.f, 0.f, WallH * 0.5f), FVector(100.f, Half * 2.f + 200.f, WallH));
	AddBlock(WallBlocks, FVector(0.f, Half + 50.f, WallH * 0.5f), FVector(Half * 2.f, 100.f, WallH));
	AddBlock(WallBlocks, FVector(0.f, -Half - 50.f, WallH * 0.5f), FVector(Half * 2.f, 100.f, WallH));

	// Structure centrale : plateforme + deux rampes.
	AddBlock(AccentBlocks, FVector(0.f, 0.f, 75.f), FVector(700.f, 700.f, 150.f));
	AddBlock(AccentBlocks, FVector(-660.f, 0.f, 70.f), FVector(680.f, 300.f, 20.f), FRotator(-12.5f, 0.f, 0.f), false);
	AddBlock(AccentBlocks, FVector(660.f, 0.f, 70.f), FVector(680.f, 300.f, 20.f), FRotator(12.5f, 0.f, 0.f), false);

	// Couvertures symétriques (équité entre équipes).
	const FVector CoverSizes[] = {
		FVector(220.f, 220.f, 120.f), FVector(100.f, 420.f, 200.f), FVector(420.f, 100.f, 200.f),
		FVector(300.f, 300.f, 320.f), FVector(160.f, 600.f, 140.f)
	};
	int32 Placed = 0;
	for (int32 Attempt = 0; Attempt < 200 && Placed < 16; ++Attempt)
	{
		const FVector Size = CoverSizes[Stream.RandRange(0, static_cast<int32>(UE_ARRAY_COUNT(CoverSizes)) - 1)];
		const FVector Pos(Stream.FRandRange(150.f, 2300.f), Stream.FRandRange(-2700.f, 2700.f), Size.Z * 0.5f);
		if (FVector2D(Pos).Size() < 750.f || IsPointBlocked(Pos, 260.f))
		{
			continue;
		}
		AddBlock(CoverBlocks, Pos, Size);
		AddBlock(CoverBlocks, FVector(-Pos.X, -Pos.Y, Pos.Z), Size);
		++Placed;
	}

	for (int32 i = 0; i < 8; ++i)
	{
		const float Y = -1750.f + i * 500.f;
		TeamSpawns0.Add(FVector(-2650.f, Y, 100.f));
		TeamSpawns1.Add(FVector(2650.f, -Y, 100.f));
	}
	for (int32 i = 0; i < 16; ++i)
	{
		FreeSpawns.Add(GetRandomOpenPoint(Stream, 350.f));
	}
	for (int32 i = 0; i < 6; ++i)
	{
		const float T = -2400.f + i * 960.f;
		ZombieSpawns.Add(FVector(T, 2850.f, 100.f));
		ZombieSpawns.Add(FVector(T, -2850.f, 100.f));
	}

	// Achats muraux (Zombies) : face au centre.
	BuyLocations.Add(FTransform(FRotator(0.f, 90.f, 0.f), FVector(-1200.f, -Half + 30.f, 140.f)));
	BuyLocations.Add(FTransform(FRotator(0.f, -90.f, 0.f), FVector(1200.f, Half - 30.f, 140.f)));
	BuyLocations.Add(FTransform(FRotator(0.f, 0.f, 0.f), FVector(-Half + 30.f, 900.f, 140.f)));
	BuyLocations.Add(FTransform(FRotator(0.f, 180.f, 0.f), FVector(Half - 30.f, -900.f, 140.f)));
	BuyLocations.Add(FTransform(FRotator(0.f, 90.f, 0.f), FVector(1200.f, -Half + 30.f, 140.f)));

	// Sites d'objectif (Tactique) côté défenseurs (équipe 1, +X).
	ObjectiveLocations.Add(FVector(1700.f, 1500.f, 5.f));
	ObjectiveLocations.Add(FVector(1700.f, -1500.f, 5.f));
}

// ---------------------------------------------------------------------------
// Grande carte 300 m (Battle Royale, Extraction)
// ---------------------------------------------------------------------------

void AStrikeArenaBuilder::BuildBuilding(FRandomStream& Stream, const FVector& Center, const FVector2D& Size, float Height)
{
	const float T = 40.f;
	const float DoorW = 220.f;
	const FVector2D H = Size * 0.5f;

	// Murs avec une porte sur deux faces opposées.
	const float SegY = (Size.Y - DoorW) * 0.5f;
	AddBlock(WallBlocks, Center + FVector(H.X, -(DoorW * 0.5f + SegY * 0.5f), Height * 0.5f), FVector(T, SegY, Height));
	AddBlock(WallBlocks, Center + FVector(H.X, (DoorW * 0.5f + SegY * 0.5f), Height * 0.5f), FVector(T, SegY, Height));
	AddBlock(WallBlocks, Center + FVector(-H.X, -(DoorW * 0.5f + SegY * 0.5f), Height * 0.5f), FVector(T, SegY, Height));
	AddBlock(WallBlocks, Center + FVector(-H.X, (DoorW * 0.5f + SegY * 0.5f), Height * 0.5f), FVector(T, SegY, Height));
	AddBlock(WallBlocks, Center + FVector(0.f, H.Y, Height * 0.5f), FVector(Size.X, T, Height));
	AddBlock(WallBlocks, Center + FVector(0.f, -H.Y, Height * 0.5f), FVector(Size.X, T, Height));
	AddBlock(FloorBlocks, Center + FVector(0.f, 0.f, Height + 15.f), FVector(Size.X + T, Size.Y + T, 30.f), FRotator::ZeroRotator, false);

	// Butin à l'intérieur et devant les portes.
	LootLocations.Add(Center + FVector(0.f, 0.f, 60.f));
	LootLocations.Add(Center + FVector(Stream.FRandRange(-H.X * 0.5f, H.X * 0.5f), Stream.FRandRange(-H.Y * 0.5f, H.Y * 0.5f), 60.f));
	LootLocations.Add(Center + FVector(H.X + 200.f, 0.f, 60.f));
	EnemySpawns.Add(Center + FVector(-H.X - 250.f, 0.f, 100.f));
}

void AStrikeArenaBuilder::BuildLargeLayout(FRandomStream& Stream)
{
	const float Half = 15000.f;
	const float WallH = 1000.f;

	AddBlock(FloorBlocks, FVector(0.f, 0.f, -50.f), FVector(Half * 2.f + 400.f, Half * 2.f + 400.f, 100.f), FRotator::ZeroRotator, false);
	AddBlock(WallBlocks, FVector(Half + 50.f, 0.f, WallH * 0.5f), FVector(100.f, Half * 2.f + 200.f, WallH));
	AddBlock(WallBlocks, FVector(-Half - 50.f, 0.f, WallH * 0.5f), FVector(100.f, Half * 2.f + 200.f, WallH));
	AddBlock(WallBlocks, FVector(0.f, Half + 50.f, WallH * 0.5f), FVector(Half * 2.f, 100.f, WallH));
	AddBlock(WallBlocks, FVector(0.f, -Half - 50.f, WallH * 0.5f), FVector(Half * 2.f, 100.f, WallH));

	// Villages : grappes de bâtiments.
	for (int32 Town = 0; Town < 7; ++Town)
	{
		const FVector TownCenter(Stream.FRandRange(-11000.f, 11000.f), Stream.FRandRange(-11000.f, 11000.f), 0.f);
		const int32 Buildings = Stream.RandRange(3, 5);
		for (int32 b = 0; b < Buildings; ++b)
		{
			const FVector2D Size(Stream.FRandRange(700.f, 1300.f), Stream.FRandRange(700.f, 1300.f));
			const FVector Pos = TownCenter + FVector(Stream.FRandRange(-2200.f, 2200.f), Stream.FRandRange(-2200.f, 2200.f), 0.f);
			if (IsPointBlocked(Pos, FMath::Max(Size.X, Size.Y) * 0.7f + 200.f))
			{
				continue;
			}
			BuildBuilding(Stream, Pos, Size, Stream.FRandRange(350.f, 450.f));
		}
	}

	// Rochers / couvertures dispersés.
	for (int32 i = 0; i < 140; ++i)
	{
		const FVector Size(Stream.FRandRange(150.f, 500.f), Stream.FRandRange(150.f, 500.f), Stream.FRandRange(100.f, 350.f));
		const FVector Pos(Stream.FRandRange(-14000.f, 14000.f), Stream.FRandRange(-14000.f, 14000.f), Size.Z * 0.5f);
		if (!IsPointBlocked(Pos, 400.f))
		{
			AddBlock(CoverBlocks, Pos, Size, FRotator(0.f, Stream.FRandRange(0.f, 90.f), 0.f));
		}
	}

	// Tours d'observation.
	for (int32 i = 0; i < 6; ++i)
	{
		const FVector Pos(Stream.FRandRange(-12000.f, 12000.f), Stream.FRandRange(-12000.f, 12000.f), 300.f);
		if (!IsPointBlocked(Pos, 600.f))
		{
			AddBlock(AccentBlocks, Pos, FVector(300.f, 300.f, 600.f));
		}
	}

	for (int32 i = 0; i < 40; ++i)
	{
		FreeSpawns.Add(GetRandomOpenPoint(Stream, 800.f));
	}
	for (int32 i = 0; i < 60; ++i)
	{
		LootLocations.Add(GetRandomOpenPoint(Stream, 500.f) - FVector(0.f, 0.f, 40.f));
	}
	for (int32 i = 0; i < 20; ++i)
	{
		EnemySpawns.Add(GetRandomOpenPoint(Stream, 1500.f));
	}
	ExtractionLocations.Add(FVector(-13000.f, -13000.f, 5.f));
	ExtractionLocations.Add(FVector(13000.f, 12500.f, 5.f));
	ExtractionLocations.Add(FVector(12500.f, -13000.f, 5.f));
}

// ---------------------------------------------------------------------------
// Stand de tir (Entraînement)
// ---------------------------------------------------------------------------

void AStrikeArenaBuilder::BuildRangeLayout(FRandomStream& Stream)
{
	// Zone : X de -1200 à 7600 (couloirs de tir), Y de -1200 à 5200 (zone bots à droite).
	const FVector FloorCenter(3200.f, 2000.f, -50.f);
	AddBlock(FloorBlocks, FloorCenter, FVector(9000.f, 6600.f, 100.f), FRotator::ZeroRotator, false);
	AddBlock(WallBlocks, FVector(7700.f, 2000.f, 400.f), FVector(100.f, 6600.f, 800.f));
	AddBlock(WallBlocks, FVector(-1300.f, 2000.f, 300.f), FVector(100.f, 6600.f, 600.f));
	AddBlock(WallBlocks, FVector(3200.f, -1300.f, 300.f), FVector(9000.f, 100.f, 600.f));
	AddBlock(WallBlocks, FVector(3200.f, 5300.f, 300.f), FVector(9000.f, 100.f, 600.f));

	// Séparation stand / zone bots.
	AddBlock(WallBlocks, FVector(3800.f, 1350.f, 200.f), FVector(7600.f, 60.f, 400.f));

	// Comptoir de tir.
	AddBlock(AccentBlocks, FVector(-200.f, 0.f, 55.f), FVector(80.f, 2200.f, 110.f));

	// Couloirs et cibles.
	const float LaneY[] = { -800.f, -270.f, 270.f, 800.f };
	const float Distances[] = { 1000.f, 2500.f, 5000.f, 7300.f };
	for (int32 Lane = 0; Lane < 4; ++Lane)
	{
		TargetLocations.Add(FVector(Distances[Lane], LaneY[Lane], 0.f));
		TargetLocations.Add(FVector(Distances[(Lane + 2) % 4], LaneY[Lane] + 120.f, 0.f));
	}
	MovingTargetLocations.Add(FVector(2000.f, 0.f, 0.f));
	MovingTargetLocations.Add(FVector(3800.f, 0.f, 0.f));

	// Râteliers d'armes derrière le comptoir.
	for (int32 i = 0; i < 6; ++i)
	{
		RackLocations.Add(FVector(-650.f, -1000.f + i * 400.f, 60.f));
	}

	// Zone de bots avec couvertures.
	for (int32 i = 0; i < 10; ++i)
	{
		const FVector Pos(Stream.FRandRange(1200.f, 6500.f), Stream.FRandRange(1900.f, 4800.f), 80.f);
		AddBlock(CoverBlocks, Pos, FVector(200.f, 200.f, 160.f));
	}
	for (int32 i = 0; i < 4; ++i)
	{
		BotLocations.Add(FVector(1800.f + i * 1200.f, 3200.f, 100.f));
	}

	for (int32 i = 0; i < 4; ++i)
	{
		FreeSpawns.Add(FVector(-700.f, -600.f + i * 400.f, 100.f));
	}
	EnemySpawns = BotLocations;
}

// ---------------------------------------------------------------------------
// Éclairage et marqueurs
// ---------------------------------------------------------------------------

void AStrikeArenaBuilder::SpawnLightingIfNeeded()
{
	if (!bSpawnLightingIfMissing || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	for (TActorIterator<ADirectionalLight> It(GetWorld()); It; ++It)
	{
		return; // Le niveau possède déjà son éclairage.
	}

	UDirectionalLightComponent* Sun = NewObject<UDirectionalLightComponent>(this, TEXT("ArenaSun"));
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->SetupAttachment(RootComponent);
	Sun->SetWorldRotation(FRotator(-48.f, 35.f, 0.f));
	Sun->SetIntensity(8.f);
	Sun->SetAtmosphereSunLight(true);
	Sun->RegisterComponent();

	USkyAtmosphereComponent* Atmosphere = NewObject<USkyAtmosphereComponent>(this, TEXT("ArenaAtmosphere"));
	Atmosphere->SetupAttachment(RootComponent);
	Atmosphere->RegisterComponent();

	UExponentialHeightFogComponent* Fog = NewObject<UExponentialHeightFogComponent>(this, TEXT("ArenaFog"));
	Fog->SetupAttachment(RootComponent);
	Fog->SetFogDensity(0.005f);
	Fog->RegisterComponent();

	USkyLightComponent* SkyLight = NewObject<USkyLightComponent>(this, TEXT("ArenaSkyLight"));
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SourceType = ESkyLightSourceType::SLS_CapturedScene;
	SkyLight->SetIntensity(1.2f);
	SkyLight->RegisterComponent();
	SkyLight->RecaptureSky();
}

void AStrikeArenaBuilder::SpawnServerMarkers()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.Owner = this;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const auto SpawnPoint = [&](const FVector& Location, EStrikeSpawnType Type, uint8 Team)
	{
		// Oriente les apparitions vers le centre de la carte.
		const FVector ToCenter = (FVector(0.f, 0.f, Location.Z) - Location).GetSafeNormal2D();
		const FRotator Rotation = ToCenter.IsNearlyZero() ? FRotator::ZeroRotator : ToCenter.Rotation();
		if (AStrikeSpawnPoint* Point = World->SpawnActor<AStrikeSpawnPoint>(AStrikeSpawnPoint::StaticClass(), Location, Rotation, Params))
		{
			Point->SpawnType = Type;
			Point->TeamId = Team;
		}
	};

	for (const FVector& L : TeamSpawns0) { SpawnPoint(L, EStrikeSpawnType::Player, 0); }
	for (const FVector& L : TeamSpawns1) { SpawnPoint(L, EStrikeSpawnType::Player, 1); }
	for (const FVector& L : FreeSpawns) { SpawnPoint(L, EStrikeSpawnType::Player, StrikeTeams::NoTeam); }
	for (const FVector& L : ZombieSpawns) { SpawnPoint(L, EStrikeSpawnType::Zombie, StrikeTeams::Zombies); }
	for (const FVector& L : EnemySpawns) { SpawnPoint(L, EStrikeSpawnType::Enemy, StrikeTeams::Hostiles); }
}

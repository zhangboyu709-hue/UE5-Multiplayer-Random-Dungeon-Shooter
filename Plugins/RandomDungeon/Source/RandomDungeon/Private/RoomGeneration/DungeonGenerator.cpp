#include "RoomGeneration/DungeonGenerator.h"
#include "RoomGeneration/Algorithms.h"
#include "Misc/DateTime.h"

ADungeonGenerator::ADungeonGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ADungeonGenerator::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (bUseRandomSeed)
	{
		const int64 Ticks = FDateTime::UtcNow().GetTicks();

		// Keep the range 1 ~ 2147483646 and avoid 0 / negative seeds.
		Seed = static_cast<int32>((Ticks % 2147483646LL) + 1LL);
	}

	UE_LOG(LogTemp, Warning, TEXT("[Dungeon] Using Seed = %d"), Seed);

	if (bGenerateOnBeginPlay)
	{
		GenerateDungeon();
	}
}

int32 ADungeonGenerator::GetTargetRoomCount()
{
	if (TotalMinRooms == -1 && TotalMaxRooms == -1)
	{
		return -1;
	}

	const int32 RequiredMinimum =
		GetRequiredMinimumRoomCount() +
		GetMinimumKeyRoomConnectorBudget();

	const int32 SafeMinRooms = FMath::Max(1, FMath::Max(TotalMinRooms, RequiredMinimum));
	const int32 SafeMaxRooms = FMath::Max(SafeMinRooms, TotalMaxRooms);

	return RandomStream.RandRange(SafeMinRooms, SafeMaxRooms);
}

int32 ADungeonGenerator::GetRequiredMinimumRoomCount() const
{
	int32 RequiredCount = 0;

	for (const FRoomSpawnRule& Rule : RoomRules)
	{
		if (Rule.RoomClass && Rule.MinCount > 0)
		{
			RequiredCount += Rule.MinCount;
		}
	}

	TSubclassOf<ABaseDungeonRoom> ActualStartRoomClass = StartRoomClass;
	if (!ActualStartRoomClass && RoomRules.Num() > 0)
	{
		ActualStartRoomClass = RoomRules[0].RoomClass;
	}

	if (ActualStartRoomClass)
	{
		for (const FRoomSpawnRule& Rule : RoomRules)
		{
			if (Rule.RoomClass == ActualStartRoomClass && Rule.MinCount > 0)
			{
				RequiredCount = FMath::Max(0, RequiredCount - 1);
				break;
			}
		}
	}

	return RequiredCount + 1;
}

int32 ADungeonGenerator::GetKeyRoomNodeCount() const
{
	if (!bUseKeyRoomMSTLayout)
	{
		return 0;
	}

	TSubclassOf<ABaseDungeonRoom> ActualStartRoomClass = StartRoomClass;
	if (!ActualStartRoomClass && RoomRules.Num() > 0)
	{
		ActualStartRoomClass = RoomRules[0].RoomClass;
	}

	if (!ActualStartRoomClass)
	{
		return 0;
	}

	int32 KeyRoomCount = 1; // Start room always belongs to the MST node set.

	for (const FRoomSpawnRule& Rule : RoomRules)
	{
		if (!Rule.bPreplaceAsKeyRoom || !Rule.RoomClass || Rule.MinCount <= 0)
		{
			continue;
		}

		int32 AdditionalCount = Rule.MinCount;
		if (Rule.RoomClass == ActualStartRoomClass)
		{
			AdditionalCount = FMath::Max(0, AdditionalCount - 1);
		}

		KeyRoomCount += AdditionalCount;
	}

	return KeyRoomCount;
}

int32 ADungeonGenerator::GetMinimumKeyRoomConnectorBudget() const
{
	return GetKeyRoomNodeCount() > 1
		? FMath::Max(0, MinimumConnectorRoomBudget)
		: 0;
}

void ADungeonGenerator::GenerateDungeon()
{
	if (!HasAuthority())
	{
		return;
	}

	if (!StartRoomClass && RoomRules.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("DungeonGenerator: No StartRoomClass or RoomRules."));
		return;
	}

	RandomStream.Initialize(Seed);

	for (int32 AttemptIndex = 0; AttemptIndex < GenerationRetryCount; ++AttemptIndex)
	{
		if (GenerateDungeonOnce(AttemptIndex))
		{
			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("DungeonGenerator: Failed after %d retries."), GenerationRetryCount);
}

bool ADungeonGenerator::GenerateDungeonOnce(int32 AttemptIndex)
{
	PlacementSystem.Initialize(GetWorld(), DungeonOrigin, CellSize);
	PlacementSystem.ClearDungeon();

	StartRoomActor = nullptr;

	TSubclassOf<ABaseDungeonRoom> ActualStartRoomClass = StartRoomClass;
	if (!ActualStartRoomClass && RoomRules.Num() > 0)
	{
		ActualStartRoomClass = RoomRules[0].RoomClass;
	}

	if (!ActualStartRoomClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("DungeonGenerator: Start room class is null."));
		return false;
	}

	if (!PlacementSystem.SpawnRoomAtAnchor(
		ActualStartRoomClass,
		FIntVector::ZeroValue,
		0,
		StartRoomActor
	))
	{
		UE_LOG(LogTemp, Warning, TEXT("DungeonGenerator: Failed to spawn start room."));
		return false;
	}

	TMap<TSubclassOf<ABaseDungeonRoom>, int32> RoomClassCounts;
	RoomClassCounts.FindOrAdd(ActualStartRoomClass)++;

	const int32 TargetRooms = GetTargetRoomCount();
	FRandomGrowthAlgorithm RandomGrowth;

	if (bUseKeyRoomMSTLayout)
	{
		FKeyRoomMSTSettings KeySettings;
		KeySettings.bEnabled = true;
		KeySettings.ConnectorRoomClass = ConnectorRoomClass;
		KeySettings.KeyRoomPlacementRadiusXY = KeyRoomPlacementRadiusXY;
		KeySettings.KeyRoomMinXYDistance = KeyRoomMinXYDistance;
		KeySettings.MaxAbsKeyRoomZ = MaxAbsKeyRoomZ;
		KeySettings.VerticalContinueProbability = KeyRoomVerticalContinueProbability;
		KeySettings.MinGridZ = MinGridZ;
		KeySettings.MaxGridZ = MaxGridZ;
		KeySettings.VerticalRouteCost = VerticalRouteCost;
		KeySettings.RouteSearchPadding = RouteSearchPadding;
		KeySettings.MaxRouteSearchNodes = MaxRouteSearchNodes;
		KeySettings.KeyRoomPlacementAttempts = KeyRoomPlacementAttempts;

		const bool bKeyLayoutSuccess = RandomGrowth.GenerateKeyRoomMSTLayout(
			PlacementSystem,
			StartRoomActor,
			FIntVector::ZeroValue,
			RoomRules,
			RoomClassCounts,
			TargetRooms,
			KeySettings,
			RandomStream
		);

		if (!bKeyLayoutSuccess)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("DungeonGenerator: Attempt %d failed during key-room MST layout."),
				AttemptIndex
			);
			return false;
		}
	}

	// This fills only any MinCount requirements that were NOT marked as key rooms.
	// Preplaced key rooms already updated RoomClassCounts, so they are skipped here.
	const bool bRequiredSuccess = RandomGrowth.GenerateRequiredRooms(
		PlacementSystem,
		RoomRules,
		RoomClassCounts,
		SafetyLimit,
		RandomStream
	);

	if (!bRequiredSuccess)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DungeonGenerator: Attempt %d failed while generating required rooms."),
			AttemptIndex
		);
		return false;
	}

	if (TargetRooms > 0 && PlacementSystem.GetPlacedRoomCount() > TargetRooms)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("DungeonGenerator: Attempt %d used %d rooms before random fill, but TargetRooms is only %d."),
			AttemptIndex,
			PlacementSystem.GetPlacedRoomCount(),
			TargetRooms
		);
		return false;
	}

	RandomGrowth.GenerateRemainingRooms(
		PlacementSystem,
		RoomRules,
		RoomClassCounts,
		TargetRooms,
		SafetyLimit,
		RandomStream
	);

	const int32 GeneratedRooms = PlacementSystem.GetPlacedRoomCount();

	if (TargetRooms > 0 && GeneratedRooms < TargetRooms)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("DungeonGenerator: Attempt %d generated only %d rooms, target was %d."),
			AttemptIndex,
			GeneratedRooms,
			TargetRooms
		);

		return false;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("DungeonGenerator: Generated %d rooms. TargetRooms=%d. Attempt=%d."),
		GeneratedRooms,
		TargetRooms,
		AttemptIndex
	);

	return true;
}

FTransform ADungeonGenerator::GetDungeonEntryTransform() const
{
	constexpr float EntryHeightOffset = 100.f;

	if (StartRoomActor)
	{
		FTransform Result = StartRoomActor->GetActorTransform();
		Result.SetLocation(StartRoomActor->GetActorLocation() + FVector(0.f, 0.f, EntryHeightOffset));
		return Result;
	}

	FTransform Result;
	Result.SetLocation(DungeonOrigin + FVector(0.f, 0.f, EntryHeightOffset));
	return Result;
}

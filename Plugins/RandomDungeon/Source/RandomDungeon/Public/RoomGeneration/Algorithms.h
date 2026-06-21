#pragma once

#include "CoreMinimal.h"
#include "RoomGeneration/RoomPlacementSystem.h"

struct FRoomSpawnRule;

/**
 * Settings for the key-room preplacement + Kruskal MST layout stage.
 * This is a plain C++ settings struct. ADungeonGenerator exposes the
 * corresponding editable UPROPERTY fields and fills this struct at runtime.
 */
struct FKeyRoomMSTSettings
{
	bool bEnabled = false;

	// Must be a ONE-CELL connector room with six Door entries:
	// directions 0..5 and non-empty DoorName values.
	TSubclassOf<ABaseDungeonRoom> ConnectorRoomClass;

	// Key rooms are sampled inside [-Radius, +Radius] on X/Y grid coordinates.
	int32 KeyRoomPlacementRadiusXY = 8;

	// Minimum X/Y Manhattan distance between any occupied cell of two key rooms.
	int32 KeyRoomMinXYDistance = 6;

	// Truncated signed geometric distribution for key-room height sampling.
	// A sample continues one more layer with this probability.
	float VerticalContinueProbability = 0.4f;
	int32 MaxAbsKeyRoomZ = 3;

	// Absolute route bounds used by A* and connector placement.
	int32 MinGridZ = -3;
	int32 MaxGridZ = 3;

	// Vertical movement is more expensive than horizontal movement in A*.
	float VerticalRouteCost = 2.0f;

	// A* bounds and safety limits.
	int32 RouteSearchPadding = 4;
	int32 MaxRouteSearchNodes = 8192;
	int32 KeyRoomPlacementAttempts = 200;
};

class RANDOMDUNGEON_API FRandomGrowthAlgorithm
{
public:
	/**
	 * Preplaces every RoomRule marked bPreplaceAsKeyRoom until its MinCount is met,
	 * then connects the start room and all key rooms with a Kruskal-style MST.
	 * The remaining MinCount and weighted random fill are intentionally performed
	 * afterwards by GenerateRequiredRooms / GenerateRemainingRooms.
	 */
	bool GenerateKeyRoomMSTLayout(
		FRoomPlacementSystem& PlacementSystem,
		ABaseDungeonRoom* StartRoomActor,
		const FIntVector& StartRoomAnchor,
		const TArray<FRoomSpawnRule>& RoomRules,
		TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
		int32 TargetRooms,
		const FKeyRoomMSTSettings& Settings,
		FRandomStream& RandomStream
	);

	bool GenerateRequiredRooms(
		FRoomPlacementSystem& PlacementSystem,
		const TArray<FRoomSpawnRule>& RoomRules,
		TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
		int32 SafetyLimit,
		FRandomStream& RandomStream
	);

	void GenerateRemainingRooms(
		FRoomPlacementSystem& PlacementSystem,
		const TArray<FRoomSpawnRule>& RoomRules,
		TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
		int32 TargetRooms,
		int32 SafetyLimit,
		FRandomStream& RandomStream
	);

private:
	bool TryPlaceSpecificRoomClass(
		FRoomPlacementSystem& PlacementSystem,
		TSubclassOf<ABaseDungeonRoom> RoomClass,
		FRandomStream& RandomStream
	) const;

	TSubclassOf<ABaseDungeonRoom> PickWeightedRoomClass(
		const TArray<FRoomSpawnRule>& RoomRules,
		const TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
		FRandomStream& RandomStream
	) const;

	bool CanUseRoomRule(
		const FRoomSpawnRule& Rule,
		const TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts
	) const;

	int32 GetRoomCount(
		TSubclassOf<ABaseDungeonRoom> RoomClass,
		const TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts
	) const;
};

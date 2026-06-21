#pragma once

#include "CoreMinimal.h"
#include "RoomGeneration/BaseDungeonRoom.h"

struct FPlacedDungeonRoom
{
	ABaseDungeonRoom* RoomActor = nullptr;
	FIntVector Anchor = FIntVector::ZeroValue;
	TArray<FIntVector> Cells;
	TArray<FRoomDoor> Doors;
};

struct FOpenDungeonDoor
{
	FIntVector RoomAnchor = FIntVector::ZeroValue;
	FRoomDoor Door;
};

class RANDOMDUNGEON_API FRoomPlacementSystem
{
public:
	void Initialize(UWorld* InWorld, const FVector& InOrigin, const FVector& InCellSize);
	void ClearDungeon();

	bool SpawnRoomAtAnchor(
		TSubclassOf<ABaseDungeonRoom> RoomClass,
		const FIntVector& Anchor,
		int32 RotationSteps,
		ABaseDungeonRoom*& OutRoomActor
	);

	bool TryPlaceRoomAtDoor(
		const FOpenDungeonDoor& OpenDoor,
		TSubclassOf<ABaseDungeonRoom> RoomClass,
		int32 RotationSteps
	);

	TArray<FOpenDungeonDoor> GetOpenDoors() const;
	int32 GetPlacedRoomCount() const;
	FTransform GetEntryTransform(float ExtraZ = 100.f) const;

private:
	UWorld* World = nullptr;
	FVector Origin = FVector::ZeroVector;
	FVector CellSize = FVector(2000.f, 2000.f, 200.f);

	TMap<FIntVector, FPlacedDungeonRoom> PlacedRooms;
	TMap<FIntVector, FIntVector> OccupiedCells;

	FVector GridToWorld(const FIntVector& Grid) const;

	FIntVector RotateCell(const FIntVector& Cell, int32 RotationSteps) const;
	int32 RotateDirection(int32 Direction, int32 RotationSteps) const;
	FRoomDoor RotateDoor(const FRoomDoor& Door, int32 RotationSteps) const;

	FIntVector DirectionToOffset(int32 Direction) const;
	int32 OppositeDirection(int32 Direction) const;
	FRotator RotationStepsToRotator(int32 RotationSteps) const;

	FPlacedDungeonRoom BuildRoomData(
		TSubclassOf<ABaseDungeonRoom> RoomClass,
		const FIntVector& Anchor,
		int32 RotationSteps
	) const;

	bool HasSameDoor(const TArray<FRoomDoor>& Doors, const FRoomDoor& NewDoor) const;
	void AddDoorUnique(TArray<FRoomDoor>& Doors, const FRoomDoor& NewDoor) const;

	bool HasDoorAtWorldCell(
		const FPlacedDungeonRoom& Room,
		const FIntVector& WorldCell,
		int32 Direction
	) const;

	bool CanPlaceRoom(const FPlacedDungeonRoom& RoomData) const;
	void RegisterPlacedRoom(const FPlacedDungeonRoom& RoomData);
};
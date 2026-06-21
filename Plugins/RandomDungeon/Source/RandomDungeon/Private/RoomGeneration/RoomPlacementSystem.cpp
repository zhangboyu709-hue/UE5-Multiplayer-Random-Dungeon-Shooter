#include "RoomGeneration/RoomPlacementSystem.h"
#include "Engine/World.h"

void FRoomPlacementSystem::Initialize(UWorld* InWorld, const FVector& InOrigin, const FVector& InCellSize)
{
	World = InWorld;
	Origin = InOrigin;
	CellSize = InCellSize;
}

void FRoomPlacementSystem::ClearDungeon()
{
	for (const TPair<FIntVector, FPlacedDungeonRoom>& Pair : PlacedRooms)
	{
		if (Pair.Value.RoomActor)
		{
			Pair.Value.RoomActor->Destroy();
		}
	}

	PlacedRooms.Empty();
	OccupiedCells.Empty();
}

int32 FRoomPlacementSystem::GetPlacedRoomCount() const
{
	return PlacedRooms.Num();
}

FTransform FRoomPlacementSystem::GetEntryTransform(float ExtraZ) const
{
	FTransform Result;
	Result.SetLocation(Origin + FVector(0.f, 0.f, CellSize.Z + ExtraZ));
	return Result;
}

FVector FRoomPlacementSystem::GridToWorld(const FIntVector& Grid) const
{
	return Origin + FVector(
		Grid.X * CellSize.X,
		Grid.Y * CellSize.Y,
		Grid.Z * CellSize.Z
	);
}

bool FRoomPlacementSystem::SpawnRoomAtAnchor(
	TSubclassOf<ABaseDungeonRoom> RoomClass,
	const FIntVector& Anchor,
	int32 RotationSteps,
	ABaseDungeonRoom*& OutRoomActor
)
{
	OutRoomActor = nullptr;

	if (!World || !RoomClass)
	{
		return false;
	}

	FPlacedDungeonRoom RoomData = BuildRoomData(RoomClass, Anchor, RotationSteps);

	if (!CanPlaceRoom(RoomData))
	{
		return false;
	}

	ABaseDungeonRoom* RoomActor = World->SpawnActor<ABaseDungeonRoom>(
		RoomClass,
		GridToWorld(Anchor),
		RotationStepsToRotator(RotationSteps)
	);

	if (!RoomActor)
	{
		return false;
	}

	RoomData.RoomActor = RoomActor;
	RegisterPlacedRoom(RoomData);

	OutRoomActor = RoomActor;
	return true;
}

bool FRoomPlacementSystem::TryPlaceRoomAtDoor(
	const FOpenDungeonDoor& OpenDoor,
	TSubclassOf<ABaseDungeonRoom> RoomClass,
	int32 RotationSteps
)
{
	if (!World || !RoomClass)
	{
		return false;
	}

	const FIntVector FromDoorWorldCell = OpenDoor.RoomAnchor + OpenDoor.Door.Cell;
	const FIntVector TargetCell = FromDoorWorldCell + DirectionToOffset(OpenDoor.Door.Direction);
	const int32 NeededDirection = OppositeDirection(OpenDoor.Door.Direction);

	FPlacedDungeonRoom CandidateBase = BuildRoomData(RoomClass, FIntVector::ZeroValue, RotationSteps);

	for (const FRoomDoor& CandidateDoor : CandidateBase.Doors)
	{
		if (CandidateDoor.Direction != NeededDirection)
		{
			continue;
		}

		const FIntVector NewAnchor = TargetCell - CandidateDoor.Cell;

		ABaseDungeonRoom* NewRoomActor = nullptr;

		if (SpawnRoomAtAnchor(RoomClass, NewAnchor, RotationSteps, NewRoomActor))
		{
			if (FPlacedDungeonRoom* FromRoom = PlacedRooms.Find(OpenDoor.RoomAnchor))
			{
				if (FromRoom->RoomActor)
				{
					FromRoom->RoomActor->OpenDoor(OpenDoor.Door);
				}
			}

			if (NewRoomActor)
			{
				NewRoomActor->OpenDoor(CandidateDoor);
			}

			return true;
		}
	}

	return false;
}

TArray<FOpenDungeonDoor> FRoomPlacementSystem::GetOpenDoors() const
{
	TArray<FOpenDungeonDoor> OpenDoors;

	for (const TPair<FIntVector, FPlacedDungeonRoom>& Pair : PlacedRooms)
	{
		const FIntVector Anchor = Pair.Key;
		const FPlacedDungeonRoom& Room = Pair.Value;

		for (const FRoomDoor& Door : Room.Doors)
		{
			const FIntVector DoorWorldCell = Anchor + Door.Cell;
			const FIntVector NeighborCell = DoorWorldCell + DirectionToOffset(Door.Direction);

			if (!OccupiedCells.Contains(NeighborCell))
			{
				FOpenDungeonDoor OpenDoor;
				OpenDoor.RoomAnchor = Anchor;
				OpenDoor.Door = Door;
				OpenDoors.Add(OpenDoor);
			}
		}
	}

	return OpenDoors;
}

FIntVector FRoomPlacementSystem::RotateCell(const FIntVector& Cell, int32 RotationSteps) const
{
	FIntVector Result = Cell;
	RotationSteps = ((RotationSteps % 4) + 4) % 4;

	for (int32 i = 0; i < RotationSteps; i++)
	{
		Result = FIntVector(-Result.Y, Result.X, Result.Z);
	}

	return Result;
}

int32 FRoomPlacementSystem::RotateDirection(int32 Direction, int32 RotationSteps) const
{
	if (Direction >= 0 && Direction <= 3)
	{
		return (Direction + RotationSteps) % 4;
	}

	return Direction;
}

FRoomDoor FRoomPlacementSystem::RotateDoor(const FRoomDoor& Door, int32 RotationSteps) const
{
	FRoomDoor Result = Door;
	Result.Cell = RotateCell(Door.Cell, RotationSteps);
	Result.Direction = RotateDirection(Door.Direction, RotationSteps);
	return Result;
}

FIntVector FRoomPlacementSystem::DirectionToOffset(int32 Direction) const
{
	switch (Direction)
	{
	case 0: return FIntVector(1, 0, 0);   // North / Forward / +X
	case 1: return FIntVector(0, 1, 0);   // East / Right / +Y
	case 2: return FIntVector(-1, 0, 0);  // South / Back / -X
	case 3: return FIntVector(0, -1, 0);  // West / Left / -Y
	case 4: return FIntVector(0, 0, 1);   // Up
	case 5: return FIntVector(0, 0, -1);  // Down
	default: return FIntVector::ZeroValue;
	}
}

int32 FRoomPlacementSystem::OppositeDirection(int32 Direction) const
{
	switch (Direction)
	{
	case 0: return 2;
	case 1: return 3;
	case 2: return 0;
	case 3: return 1;
	case 4: return 5;
	case 5: return 4;
	default: return 0;
	}
}

FRotator FRoomPlacementSystem::RotationStepsToRotator(int32 RotationSteps) const
{
	return FRotator(0.f, RotationSteps * 90.f, 0.f);
}

FPlacedDungeonRoom FRoomPlacementSystem::BuildRoomData(
	TSubclassOf<ABaseDungeonRoom> RoomClass,
	const FIntVector& Anchor,
	int32 RotationSteps
) const
{
	FPlacedDungeonRoom Data;
	Data.Anchor = Anchor;

	if (!RoomClass)
	{
		return Data;
	}

	const ABaseDungeonRoom* DefaultRoom = RoomClass->GetDefaultObject<ABaseDungeonRoom>();

	if (!DefaultRoom)
	{
		return Data;
	}

	for (const FIntVector& Cell : DefaultRoom->Cells)
	{
		Data.Cells.Add(RotateCell(Cell, RotationSteps));
	}

	for (const FRoomDoor& Door : DefaultRoom->Doors)
	{
		AddDoorUnique(Data.Doors, RotateDoor(Door, RotationSteps));
	}

	return Data;
}

bool FRoomPlacementSystem::HasSameDoor(const TArray<FRoomDoor>& Doors, const FRoomDoor& NewDoor) const
{
	for (const FRoomDoor& Door : Doors)
	{
		if (Door.Cell == NewDoor.Cell && Door.Direction == NewDoor.Direction)
		{
			return true;
		}
	}

	return false;
}

void FRoomPlacementSystem::AddDoorUnique(TArray<FRoomDoor>& Doors, const FRoomDoor& NewDoor) const
{
	if (!HasSameDoor(Doors, NewDoor))
	{
		Doors.Add(NewDoor);
	}
}

bool FRoomPlacementSystem::HasDoorAtWorldCell(
	const FPlacedDungeonRoom& Room,
	const FIntVector& WorldCell,
	int32 Direction
) const
{
	const FIntVector LocalCell = WorldCell - Room.Anchor;

	for (const FRoomDoor& Door : Room.Doors)
	{
		if (Door.Cell == LocalCell && Door.Direction == Direction)
		{
			return true;
		}
	}

	return false;
}

bool FRoomPlacementSystem::CanPlaceRoom(const FPlacedDungeonRoom& RoomData) const
{
	TSet<FIntVector> NewWorldCells;

	for (const FIntVector& LocalCell : RoomData.Cells)
	{
		const FIntVector WorldCell = RoomData.Anchor + LocalCell;

		if (OccupiedCells.Contains(WorldCell))
		{
			return false;
		}

		NewWorldCells.Add(WorldCell);
	}

	for (const FIntVector& LocalCell : RoomData.Cells)
	{
		const FIntVector WorldCell = RoomData.Anchor + LocalCell;

		for (int32 Direction = 0; Direction < 6; Direction++)
		{
			const FIntVector NeighborCell = WorldCell + DirectionToOffset(Direction);

			if (NewWorldCells.Contains(NeighborCell))
			{
				continue;
			}

			const FIntVector* NeighborAnchor = OccupiedCells.Find(NeighborCell);

			if (!NeighborAnchor)
			{
				continue;
			}

			const FPlacedDungeonRoom* NeighborRoom = PlacedRooms.Find(*NeighborAnchor);

			if (!NeighborRoom)
			{
				continue;
			}

			const bool bThisHasDoor = HasDoorAtWorldCell(RoomData, WorldCell, Direction);
			const bool bNeighborHasDoor = HasDoorAtWorldCell(
				*NeighborRoom,
				NeighborCell,
				OppositeDirection(Direction)
			);

			if (bThisHasDoor != bNeighborHasDoor)
			{
				return false;
			}
		}
	}

	return true;
}

void FRoomPlacementSystem::RegisterPlacedRoom(const FPlacedDungeonRoom& RoomData)
{
	PlacedRooms.Add(RoomData.Anchor, RoomData);

	for (const FIntVector& LocalCell : RoomData.Cells)
	{
		const FIntVector WorldCell = RoomData.Anchor + LocalCell;
		OccupiedCells.Add(WorldCell, RoomData.Anchor);
	}
}
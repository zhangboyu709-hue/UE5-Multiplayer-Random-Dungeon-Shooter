#include "RoomGeneration/Algorithms.h"
#include "RoomGeneration/DungeonGenerator.h"

#include "Algo/Reverse.h"
#include "Algo/Sort.h"

namespace
{
	struct FKeyRoomNode
	{
		ABaseDungeonRoom* Actor = nullptr;
		TSubclassOf<ABaseDungeonRoom> RoomClass;
		FIntVector Anchor = FIntVector::ZeroValue;
		int32 RotationSteps = 0;

		TArray<FIntVector> WorldCells;

		// Local values are kept because ABaseDungeonRoom::OpenDoor() opens by DoorName.
		TArray<FRoomDoor> LocalDoors;
		TArray<FIntVector> WorldDoorCells;
		TArray<int32> WorldDoorDirections;
		TSet<int32> UsedDoorIndices;
	};

	struct FKeyEdge
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		float EstimatedCost = 0.0f;
	};

	struct FDoorPairCandidate
	{
		int32 DoorA = INDEX_NONE;
		int32 DoorB = INDEX_NONE;
		float EstimatedCost = 0.0f;
	};

	struct FRouteChoice
	{
		int32 DoorA = INDEX_NONE;
		int32 DoorB = INDEX_NONE;
		bool bDirectConnection = false;
		TArray<FIntVector> PathCells;
	};

	enum class ERouteCommitResult : uint8
	{
		Success,
		NotFeasible,
		HardFailure
	};

	struct FDisjointSet
	{
		TArray<int32> Parent;
		TArray<int32> Rank;

		explicit FDisjointSet(int32 Count)
		{
			Parent.SetNum(Count);
			Rank.Init(0, Count);

			for (int32 Index = 0; Index < Count; ++Index)
			{
				Parent[Index] = Index;
			}
		}

		int32 Find(int32 Value)
		{
			if (Parent[Value] != Value)
			{
				Parent[Value] = Find(Parent[Value]);
			}
			return Parent[Value];
		}

		bool Union(int32 A, int32 B)
		{
			int32 RootA = Find(A);
			int32 RootB = Find(B);

			if (RootA == RootB)
			{
				return false;
			}

			if (Rank[RootA] < Rank[RootB])
			{
				Swap(RootA, RootB);
			}

			Parent[RootB] = RootA;
			if (Rank[RootA] == Rank[RootB])
			{
				++Rank[RootA];
			}

			return true;
		}
	};

	FIntVector RotateCell(const FIntVector& Cell, int32 RotationSteps)
	{
		FIntVector Result = Cell;
		RotationSteps = ((RotationSteps % 4) + 4) % 4;

		for (int32 Index = 0; Index < RotationSteps; ++Index)
		{
			Result = FIntVector(-Result.Y, Result.X, Result.Z);
		}

		return Result;
	}

	int32 RotateDirection(int32 Direction, int32 RotationSteps)
	{
		if (Direction >= 0 && Direction <= 3)
		{
			RotationSteps = ((RotationSteps % 4) + 4) % 4;
			return (Direction + RotationSteps) % 4;
		}

		return Direction;
	}

	FIntVector DirectionToOffset(int32 Direction)
	{
		switch (Direction)
		{
		case 0: return FIntVector(1, 0, 0);   // +X
		case 1: return FIntVector(0, 1, 0);   // +Y
		case 2: return FIntVector(-1, 0, 0);  // -X
		case 3: return FIntVector(0, -1, 0);  // -Y
		case 4: return FIntVector(0, 0, 1);   // +Z
		case 5: return FIntVector(0, 0, -1);  // -Z
		default: return FIntVector::ZeroValue;
		}
	}

	int32 OppositeDirection(int32 Direction)
	{
		switch (Direction)
		{
		case 0: return 2;
		case 1: return 3;
		case 2: return 0;
		case 3: return 1;
		case 4: return 5;
		case 5: return 4;
		default: return INDEX_NONE;
		}
	}

	int32 DirectionFromDelta(const FIntVector& Delta)
	{
		for (int32 Direction = 0; Direction < 6; ++Direction)
		{
			if (DirectionToOffset(Direction) == Delta)
			{
				return Direction;
			}
		}

		return INDEX_NONE;
	}

	bool BuildKeyRoomNode(
		TSubclassOf<ABaseDungeonRoom> RoomClass,
		ABaseDungeonRoom* Actor,
		const FIntVector& Anchor,
		int32 RotationSteps,
		FKeyRoomNode& OutNode
	)
	{
		const ABaseDungeonRoom* DefaultRoom = RoomClass
			? RoomClass->GetDefaultObject<ABaseDungeonRoom>()
			: nullptr;

		if (!DefaultRoom)
		{
			return false;
		}

		OutNode = FKeyRoomNode();
		OutNode.Actor = Actor;
		OutNode.RoomClass = RoomClass;
		OutNode.Anchor = Anchor;
		OutNode.RotationSteps = RotationSteps;

		for (const FIntVector& LocalCell : DefaultRoom->Cells)
		{
			OutNode.WorldCells.Add(Anchor + RotateCell(LocalCell, RotationSteps));
		}

		for (const FRoomDoor& LocalDoor : DefaultRoom->Doors)
		{
			OutNode.LocalDoors.Add(LocalDoor);
			OutNode.WorldDoorCells.Add(Anchor + RotateCell(LocalDoor.Cell, RotationSteps));
			OutNode.WorldDoorDirections.Add(RotateDirection(LocalDoor.Direction, RotationSteps));
		}

		return OutNode.WorldCells.Num() > 0;
	}

	int32 GetSignedGeometricZ(FRandomStream& RandomStream, const FKeyRoomMSTSettings& Settings)
	{
		const int32 MaxAbsZ = FMath::Max(0, Settings.MaxAbsKeyRoomZ);
		if (MaxAbsZ == 0)
		{
			return 0;
		}

		const float ContinueProbability = FMath::Clamp(Settings.VerticalContinueProbability, 0.0f, 0.99f);

		// Rejection sampling creates a truncated geometric distribution rather than
		// pushing all overflow probability mass onto the top layer.
		for (int32 Retry = 0; Retry < 128; ++Retry)
		{
			int32 Level = 0;
			bool bOverflow = false;

			while (RandomStream.FRand() < ContinueProbability)
			{
				++Level;
				if (Level > MaxAbsZ)
				{
					bOverflow = true;
					break;
				}
			}

			if (!bOverflow)
			{
				if (Level == 0)
				{
					return 0;
				}

				return (RandomStream.FRand() < 0.5f) ? Level : -Level;
			}
		}

		return 0;
	}

	bool HasMinimumXYDistance(
		const FKeyRoomNode& Candidate,
		const TArray<FKeyRoomNode>& ExistingNodes,
		int32 MinimumXYDistance
	)
	{
		if (MinimumXYDistance <= 0)
		{
			return true;
		}

		for (const FKeyRoomNode& Existing : ExistingNodes)
		{
			for (const FIntVector& CandidateCell : Candidate.WorldCells)
			{
				for (const FIntVector& ExistingCell : Existing.WorldCells)
				{
					const int32 XYDistance =
						FMath::Abs(CandidateCell.X - ExistingCell.X) +
						FMath::Abs(CandidateCell.Y - ExistingCell.Y);

					if (XYDistance < MinimumXYDistance)
					{
						return false;
					}
				}
			}
		}

		return true;
	}

	float EstimateNodeDistance(
		const FKeyRoomNode& A,
		const FKeyRoomNode& B,
		const FKeyRoomMSTSettings& Settings
	)
	{
		float Best = TNumericLimits<float>::Max();

		for (const FIntVector& CellA : A.WorldCells)
		{
			for (const FIntVector& CellB : B.WorldCells)
			{
				const float Horizontal = static_cast<float>(
					FMath::Abs(CellA.X - CellB.X) +
					FMath::Abs(CellA.Y - CellB.Y)
					);
				const float Vertical = static_cast<float>(FMath::Abs(CellA.Z - CellB.Z));
				Best = FMath::Min(Best, Horizontal + Settings.VerticalRouteCost * Vertical);
			}
		}

		return Best;
	}

	TSet<FIntVector> BuildReservedCells(const TArray<FKeyRoomNode>& Nodes)
	{
		TSet<FIntVector> Result;
		for (const FKeyRoomNode& Node : Nodes)
		{
			for (const FIntVector& Cell : Node.WorldCells)
			{
				Result.Add(Cell);
			}
		}
		return Result;
	}

	bool IsInsideRouteBounds(
		const FIntVector& Cell,
		const FIntVector& Start,
		const FIntVector& Goal,
		const FKeyRoomMSTSettings& Settings
	)
	{
		const int32 Padding = FMath::Max(0, Settings.RouteSearchPadding);
		const int32 MinX = FMath::Min(Start.X, Goal.X) - Padding;
		const int32 MaxX = FMath::Max(Start.X, Goal.X) + Padding;
		const int32 MinY = FMath::Min(Start.Y, Goal.Y) - Padding;
		const int32 MaxY = FMath::Max(Start.Y, Goal.Y) + Padding;

		return
			Cell.X >= MinX && Cell.X <= MaxX &&
			Cell.Y >= MinY && Cell.Y <= MaxY &&
			Cell.Z >= Settings.MinGridZ && Cell.Z <= Settings.MaxGridZ;
	}

	bool CanUseRouteCell(
		const FIntVector& Candidate,
		const FIntVector& Start,
		const FIntVector& Goal,
		const FIntVector& SourceDoorWorldCell,
		const FIntVector& TargetDoorWorldCell,
		const TSet<FIntVector>& ReservedCells,
		const FKeyRoomMSTSettings& Settings
	)
	{
		if (!IsInsideRouteBounds(Candidate, Start, Goal, Settings))
		{
			return false;
		}

		if (ReservedCells.Contains(Candidate))
		{
			return false;
		}

		// Keep every routed connector cell one-cell away from existing rooms/corridors,
		// except for its intended source/target door attachment. This is what makes the
		// generic 6-door connector room safe with the current CanPlaceRoom() checks.
		for (int32 Direction = 0; Direction < 6; ++Direction)
		{
			const FIntVector Neighbor = Candidate + DirectionToOffset(Direction);
			if (!ReservedCells.Contains(Neighbor))
			{
				continue;
			}

			const bool bAllowedSourceAttachment =
				Candidate == Start && Neighbor == SourceDoorWorldCell;
			const bool bAllowedTargetAttachment =
				Candidate == Goal && Neighbor == TargetDoorWorldCell;

			if (!bAllowedSourceAttachment && !bAllowedTargetAttachment)
			{
				return false;
			}
		}

		return true;
	}

	int32 GetHorizontalStepCost()
	{
		return 10;
	}

	int32 GetVerticalStepCost(const FKeyRoomMSTSettings& Settings)
	{
		return FMath::Max(10, FMath::RoundToInt(Settings.VerticalRouteCost * 10.0f));
	}

	int32 GetAStarHeuristic(
		const FIntVector& From,
		const FIntVector& Goal,
		const FKeyRoomMSTSettings& Settings
	)
	{
		const int32 Horizontal = FMath::Abs(From.X - Goal.X) + FMath::Abs(From.Y - Goal.Y);
		const int32 Vertical = FMath::Abs(From.Z - Goal.Z);
		return Horizontal * GetHorizontalStepCost() + Vertical * GetVerticalStepCost(Settings);
	}

	bool FindAStarPath(
		const FIntVector& Start,
		const FIntVector& Goal,
		const FIntVector& SourceDoorWorldCell,
		const FIntVector& TargetDoorWorldCell,
		const TSet<FIntVector>& ReservedCells,
		const FKeyRoomMSTSettings& Settings,
		TArray<FIntVector>& OutPath
	)
	{
		OutPath.Reset();

		if (!CanUseRouteCell(Start, Start, Goal, SourceDoorWorldCell, TargetDoorWorldCell, ReservedCells, Settings) ||
			!CanUseRouteCell(Goal, Start, Goal, SourceDoorWorldCell, TargetDoorWorldCell, ReservedCells, Settings))
		{
			return false;
		}

		TArray<FIntVector> OpenList;
		TSet<FIntVector> ClosedSet;
		TMap<FIntVector, int32> GScore;
		TMap<FIntVector, FIntVector> CameFrom;

		OpenList.Add(Start);
		GScore.Add(Start, 0);

		int32 ExpandedNodes = 0;
		const int32 MaxNodes = FMath::Max(128, Settings.MaxRouteSearchNodes);

		while (OpenList.Num() > 0 && ExpandedNodes < MaxNodes)
		{
			int32 BestIndex = 0;
			int32 BestFScore = TNumericLimits<int32>::Max();

			for (int32 Index = 0; Index < OpenList.Num(); ++Index)
			{
				const FIntVector& Candidate = OpenList[Index];
				const int32* CandidateG = GScore.Find(Candidate);
				if (!CandidateG)
				{
					continue;
				}

				const int32 FScore = *CandidateG + GetAStarHeuristic(Candidate, Goal, Settings);
				if (FScore < BestFScore)
				{
					BestFScore = FScore;
					BestIndex = Index;
				}
			}

			const FIntVector Current = OpenList[BestIndex];
			OpenList.RemoveAtSwap(BestIndex);

			if (ClosedSet.Contains(Current))
			{
				continue;
			}

			if (Current == Goal)
			{
				FIntVector Trace = Goal;
				OutPath.Add(Trace);

				while (Trace != Start)
				{
					const FIntVector* Previous = CameFrom.Find(Trace);
					if (!Previous)
					{
						OutPath.Reset();
						return false;
					}

					Trace = *Previous;
					OutPath.Add(Trace);
				}

				Algo::Reverse(OutPath);
				return true;
			}

			ClosedSet.Add(Current);
			++ExpandedNodes;

			const int32* CurrentG = GScore.Find(Current);
			if (!CurrentG)
			{
				continue;
			}

			for (int32 Direction = 0; Direction < 6; ++Direction)
			{
				const FIntVector Next = Current + DirectionToOffset(Direction);

				if (ClosedSet.Contains(Next) ||
					!CanUseRouteCell(Next, Start, Goal, SourceDoorWorldCell, TargetDoorWorldCell, ReservedCells, Settings))
				{
					continue;
				}

				const int32 StepCost = (Direction <= 3)
					? GetHorizontalStepCost()
					: GetVerticalStepCost(Settings);
				const int32 TentativeG = *CurrentG + StepCost;

				const int32* ExistingG = GScore.Find(Next);
				if (!ExistingG || TentativeG < *ExistingG)
				{
					GScore.Add(Next, TentativeG);
					CameFrom.Add(Next, Current);
					OpenList.AddUnique(Next);
				}
			}
		}

		return false;
	}

	bool FindConnectorDoor(ABaseDungeonRoom* ConnectorActor, int32 Direction, FRoomDoor& OutDoor)
	{
		if (!ConnectorActor)
		{
			return false;
		}

		for (const FRoomDoor& Door : ConnectorActor->Doors)
		{
			if (Door.Cell == FIntVector::ZeroValue &&
				Door.Direction == Direction &&
				!Door.DoorName.IsNone())
			{
				OutDoor = Door;
				return true;
			}
		}

		return false;
	}

	bool ValidateConnectorRoomClass(TSubclassOf<ABaseDungeonRoom> ConnectorRoomClass)
	{
		if (!ConnectorRoomClass)
		{
			return false;
		}

		const ABaseDungeonRoom* DefaultConnector = ConnectorRoomClass->GetDefaultObject<ABaseDungeonRoom>();
		if (!DefaultConnector || DefaultConnector->Cells.Num() != 1 || DefaultConnector->Cells[0] != FIntVector::ZeroValue)
		{
			return false;
		}

		for (int32 Direction = 0; Direction < 6; ++Direction)
		{
			bool bFoundDoor = false;
			for (const FRoomDoor& Door : DefaultConnector->Doors)
			{
				if (Door.Cell == FIntVector::ZeroValue &&
					Door.Direction == Direction &&
					!Door.DoorName.IsNone())
				{
					bFoundDoor = true;
					break;
				}
			}

			if (!bFoundDoor)
			{
				return false;
			}
		}

		return true;
	}

	bool BuildBestDoorRoute(
		const FKeyRoomNode& A,
		const FKeyRoomNode& B,
		const TSet<FIntVector>& ReservedCells,
		const FKeyRoomMSTSettings& Settings,
		FRouteChoice& OutChoice
	)
	{
		OutChoice = FRouteChoice();

		TArray<FDoorPairCandidate> Candidates;

		for (int32 DoorA = 0; DoorA < A.LocalDoors.Num(); ++DoorA)
		{
			if (A.UsedDoorIndices.Contains(DoorA))
			{
				continue;
			}

			if (A.LocalDoors[DoorA].DoorName.IsNone())
			{
				continue;
			}

			for (int32 DoorB = 0; DoorB < B.LocalDoors.Num(); ++DoorB)
			{
				if (B.UsedDoorIndices.Contains(DoorB) || B.LocalDoors[DoorB].DoorName.IsNone())
				{
					continue;
				}

				const FIntVector Start = A.WorldDoorCells[DoorA] + DirectionToOffset(A.WorldDoorDirections[DoorA]);
				const FIntVector Goal = B.WorldDoorCells[DoorB] + DirectionToOffset(B.WorldDoorDirections[DoorB]);

				const float Horizontal = static_cast<float>(
					FMath::Abs(Start.X - Goal.X) + FMath::Abs(Start.Y - Goal.Y)
					);
				const float Vertical = static_cast<float>(FMath::Abs(Start.Z - Goal.Z));

				FDoorPairCandidate Candidate;
				Candidate.DoorA = DoorA;
				Candidate.DoorB = DoorB;
				Candidate.EstimatedCost = Horizontal + Settings.VerticalRouteCost * Vertical;
				Candidates.Add(Candidate);
			}
		}

		Candidates.Sort([](const FDoorPairCandidate& Left, const FDoorPairCandidate& Right)
			{
				return Left.EstimatedCost < Right.EstimatedCost;
			});

		for (const FDoorPairCandidate& Candidate : Candidates)
		{
			const int32 SourceDirection = A.WorldDoorDirections[Candidate.DoorA];
			const int32 TargetDirection = B.WorldDoorDirections[Candidate.DoorB];
			const FIntVector SourceDoorCell = A.WorldDoorCells[Candidate.DoorA];
			const FIntVector TargetDoorCell = B.WorldDoorCells[Candidate.DoorB];
			const FIntVector Start = SourceDoorCell + DirectionToOffset(SourceDirection);
			const FIntVector Goal = TargetDoorCell + DirectionToOffset(TargetDirection);

			if (Start == TargetDoorCell && SourceDirection == OppositeDirection(TargetDirection))
			{
				OutChoice.DoorA = Candidate.DoorA;
				OutChoice.DoorB = Candidate.DoorB;
				OutChoice.bDirectConnection = true;
				return true;
			}

			TArray<FIntVector> Path;
			if (FindAStarPath(Start, Goal, SourceDoorCell, TargetDoorCell, ReservedCells, Settings, Path))
			{
				OutChoice.DoorA = Candidate.DoorA;
				OutChoice.DoorB = Candidate.DoorB;
				OutChoice.bDirectConnection = false;
				OutChoice.PathCells = MoveTemp(Path);
				return true;
			}
		}

		return false;
	}

	bool OpenDoorPair(
		ABaseDungeonRoom* A,
		const FRoomDoor& DoorA,
		ABaseDungeonRoom* B,
		const FRoomDoor& DoorB
	)
	{
		if (!A || !B || DoorA.DoorName.IsNone() || DoorB.DoorName.IsNone())
		{
			return false;
		}

		A->OpenDoor(DoorA);
		B->OpenDoor(DoorB);
		return true;
	}

	ERouteCommitResult TryCommitKeyRoute(
		FRoomPlacementSystem& PlacementSystem,
		FKeyRoomNode& A,
		FKeyRoomNode& B,
		TSet<FIntVector>& ReservedCells,
		int32 TargetRooms,
		const FKeyRoomMSTSettings& Settings
	)
	{
		FRouteChoice Choice;
		if (!BuildBestDoorRoute(A, B, ReservedCells, Settings, Choice))
		{
			return ERouteCommitResult::NotFeasible;
		}

		if (Choice.bDirectConnection)
		{
			if (!OpenDoorPair(A.Actor, A.LocalDoors[Choice.DoorA], B.Actor, B.LocalDoors[Choice.DoorB]))
			{
				return ERouteCommitResult::HardFailure;
			}

			A.UsedDoorIndices.Add(Choice.DoorA);
			B.UsedDoorIndices.Add(Choice.DoorB);
			return ERouteCommitResult::Success;
		}

		if (!ValidateConnectorRoomClass(Settings.ConnectorRoomClass))
		{
			return ERouteCommitResult::HardFailure;
		}

		if (TargetRooms > 0 && PlacementSystem.GetPlacedRoomCount() + Choice.PathCells.Num() > TargetRooms)
		{
			return ERouteCommitResult::NotFeasible;
		}

		TArray<ABaseDungeonRoom*> ConnectorActors;
		ConnectorActors.Reserve(Choice.PathCells.Num());

		for (const FIntVector& PathCell : Choice.PathCells)
		{
			ABaseDungeonRoom* ConnectorActor = nullptr;
			if (!PlacementSystem.SpawnRoomAtAnchor(
				Settings.ConnectorRoomClass,
				PathCell,
				0,
				ConnectorActor
			))
			{
				// Actors already spawned on this route will be cleared by the outer
				// GenerationRetryCount retry. Do not continue with a partial route.
				return ERouteCommitResult::HardFailure;
			}

			ConnectorActors.Add(ConnectorActor);
		}

		const int32 SourceDirection = A.WorldDoorDirections[Choice.DoorA];
		const int32 TargetDirection = B.WorldDoorDirections[Choice.DoorB];

		FRoomDoor FirstConnectorDoor;
		if (!FindConnectorDoor(ConnectorActors[0], OppositeDirection(SourceDirection), FirstConnectorDoor) ||
			!OpenDoorPair(A.Actor, A.LocalDoors[Choice.DoorA], ConnectorActors[0], FirstConnectorDoor))
		{
			return ERouteCommitResult::HardFailure;
		}

		for (int32 Index = 0; Index + 1 < ConnectorActors.Num(); ++Index)
		{
			const int32 Direction = DirectionFromDelta(Choice.PathCells[Index + 1] - Choice.PathCells[Index]);
			if (Direction == INDEX_NONE)
			{
				return ERouteCommitResult::HardFailure;
			}

			FRoomDoor CurrentDoor;
			FRoomDoor NextDoor;
			if (!FindConnectorDoor(ConnectorActors[Index], Direction, CurrentDoor) ||
				!FindConnectorDoor(ConnectorActors[Index + 1], OppositeDirection(Direction), NextDoor) ||
				!OpenDoorPair(ConnectorActors[Index], CurrentDoor, ConnectorActors[Index + 1], NextDoor))
			{
				return ERouteCommitResult::HardFailure;
			}
		}

		FRoomDoor LastConnectorDoor;
		if (!FindConnectorDoor(ConnectorActors.Last(), OppositeDirection(TargetDirection), LastConnectorDoor) ||
			!OpenDoorPair(ConnectorActors.Last(), LastConnectorDoor, B.Actor, B.LocalDoors[Choice.DoorB]))
		{
			return ERouteCommitResult::HardFailure;
		}

		for (const FIntVector& PathCell : Choice.PathCells)
		{
			ReservedCells.Add(PathCell);
		}

		A.UsedDoorIndices.Add(Choice.DoorA);
		B.UsedDoorIndices.Add(Choice.DoorB);
		return ERouteCommitResult::Success;
	}

	bool BuildAndRouteKruskalMST(
		FRoomPlacementSystem& PlacementSystem,
		TArray<FKeyRoomNode>& KeyNodes,
		TSet<FIntVector>& ReservedCells,
		int32 TargetRooms,
		const FKeyRoomMSTSettings& Settings
	)
	{
		if (KeyNodes.Num() <= 1)
		{
			return true;
		}

		TArray<FKeyEdge> Edges;
		for (int32 A = 0; A < KeyNodes.Num(); ++A)
		{
			for (int32 B = A + 1; B < KeyNodes.Num(); ++B)
			{
				FKeyEdge Edge;
				Edge.A = A;
				Edge.B = B;
				Edge.EstimatedCost = EstimateNodeDistance(KeyNodes[A], KeyNodes[B], Settings);
				Edges.Add(Edge);
			}
		}

		Edges.Sort([](const FKeyEdge& Left, const FKeyEdge& Right)
			{
				return Left.EstimatedCost < Right.EstimatedCost;
			});

		FDisjointSet Sets(KeyNodes.Num());
		int32 ConnectedEdges = 0;

		for (const FKeyEdge& Edge : Edges)
		{
			if (Sets.Find(Edge.A) == Sets.Find(Edge.B))
			{
				continue;
			}

			const ERouteCommitResult Result = TryCommitKeyRoute(
				PlacementSystem,
				KeyNodes[Edge.A],
				KeyNodes[Edge.B],
				ReservedCells,
				TargetRooms,
				Settings
			);

			if (Result == ERouteCommitResult::HardFailure)
			{
				return false;
			}

			if (Result == ERouteCommitResult::Success)
			{
				Sets.Union(Edge.A, Edge.B);
				++ConnectedEdges;

				if (ConnectedEdges == KeyNodes.Num() - 1)
				{
					return true;
				}
			}
		}

		return false;
	}
}

bool FRandomGrowthAlgorithm::GenerateKeyRoomMSTLayout(
	FRoomPlacementSystem& PlacementSystem,
	ABaseDungeonRoom* StartRoomActor,
	const FIntVector& StartRoomAnchor,
	const TArray<FRoomSpawnRule>& RoomRules,
	TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
	int32 TargetRooms,
	const FKeyRoomMSTSettings& Settings,
	FRandomStream& RandomStream
)
{
	if (!Settings.bEnabled)
	{
		return true;
	}

	if (!StartRoomActor)
	{
		return false;
	}

	TSubclassOf<ABaseDungeonRoom> StartRoomClass = StartRoomActor->GetClass();
	FKeyRoomNode StartNode;
	if (!BuildKeyRoomNode(StartRoomClass, StartRoomActor, StartRoomAnchor, 0, StartNode))
	{
		return false;
	}

	TArray<FKeyRoomNode> KeyNodes;
	KeyNodes.Add(StartNode);

	const int32 AttemptsPerRoom = FMath::Max(1, Settings.KeyRoomPlacementAttempts);

	for (const FRoomSpawnRule& Rule : RoomRules)
	{
		if (!Rule.bPreplaceAsKeyRoom || !Rule.RoomClass || Rule.MinCount <= 0)
		{
			continue;
		}

		while (GetRoomCount(Rule.RoomClass, RoomClassCounts) < Rule.MinCount)
		{
			bool bPlaced = false;

			for (int32 Attempt = 0; Attempt < AttemptsPerRoom; ++Attempt)
			{
				const FIntVector CandidateAnchor(
					RandomStream.RandRange(-Settings.KeyRoomPlacementRadiusXY, Settings.KeyRoomPlacementRadiusXY),
					RandomStream.RandRange(-Settings.KeyRoomPlacementRadiusXY, Settings.KeyRoomPlacementRadiusXY),
					GetSignedGeometricZ(RandomStream, Settings)
				);

				if (CandidateAnchor.Z < Settings.MinGridZ || CandidateAnchor.Z > Settings.MaxGridZ)
				{
					continue;
				}

				const int32 RotationSteps = RandomStream.RandRange(0, 3);
				FKeyRoomNode CandidateNode;
				if (!BuildKeyRoomNode(Rule.RoomClass, nullptr, CandidateAnchor, RotationSteps, CandidateNode))
				{
					continue;
				}

				if (!HasMinimumXYDistance(CandidateNode, KeyNodes, Settings.KeyRoomMinXYDistance))
				{
					continue;
				}

				ABaseDungeonRoom* SpawnedRoom = nullptr;
				if (!PlacementSystem.SpawnRoomAtAnchor(Rule.RoomClass, CandidateAnchor, RotationSteps, SpawnedRoom))
				{
					continue;
				}

				CandidateNode.Actor = SpawnedRoom;
				KeyNodes.Add(CandidateNode);
				int32& Count = RoomClassCounts.FindOrAdd(Rule.RoomClass);
				++Count;
				bPlaced = true;
				break;
			}

			if (!bPlaced)
			{
				return false;
			}
		}
	}

	if (KeyNodes.Num() <= 1)
	{
		return true;
	}

	if (!ValidateConnectorRoomClass(Settings.ConnectorRoomClass))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("KeyRoomMST: ConnectorRoomClass must be one cell with six valid DoorSlots (directions 0..5)."));
		return false;
	}

	TSet<FIntVector> ReservedCells = BuildReservedCells(KeyNodes);
	return BuildAndRouteKruskalMST(PlacementSystem, KeyNodes, ReservedCells, TargetRooms, Settings);
}

bool FRandomGrowthAlgorithm::GenerateRequiredRooms(
	FRoomPlacementSystem& PlacementSystem,
	const TArray<FRoomSpawnRule>& RoomRules,
	TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
	int32 SafetyLimit,
	FRandomStream& RandomStream
)
{
	int32 SafetyCounter = 0;

	for (const FRoomSpawnRule& Rule : RoomRules)
	{
		if (!Rule.RoomClass || Rule.MinCount <= 0)
		{
			continue;
		}

		while (GetRoomCount(Rule.RoomClass, RoomClassCounts) < Rule.MinCount)
		{
			if (SafetyCounter++ >= SafetyLimit)
			{
				return false;
			}

			if (!CanUseRoomRule(Rule, RoomClassCounts))
			{
				return false;
			}

			const bool bPlaced = TryPlaceSpecificRoomClass(
				PlacementSystem,
				Rule.RoomClass,
				RandomStream
			);

			if (!bPlaced)
			{
				return false;
			}

			int32& Count = RoomClassCounts.FindOrAdd(Rule.RoomClass);
			++Count;
		}
	}

	return true;
}

void FRandomGrowthAlgorithm::GenerateRemainingRooms(
	FRoomPlacementSystem& PlacementSystem,
	const TArray<FRoomSpawnRule>& RoomRules,
	TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
	int32 TargetRooms,
	int32 SafetyLimit,
	FRandomStream& RandomStream
)
{
	if (RoomRules.Num() == 0)
	{
		return;
	}

	const bool bInfinite = TargetRooms < 0;
	int32 SafetyCounter = 0;

	while (
		(bInfinite || PlacementSystem.GetPlacedRoomCount() < TargetRooms) &&
		SafetyCounter < SafetyLimit
		)
	{
		++SafetyCounter;

		TArray<FOpenDungeonDoor> OpenDoors = PlacementSystem.GetOpenDoors();
		if (OpenDoors.Num() == 0)
		{
			break;
		}

		const int32 OpenDoorIndex = RandomStream.RandRange(0, OpenDoors.Num() - 1);
		const FOpenDungeonDoor SelectedOpenDoor = OpenDoors[OpenDoorIndex];

		TSubclassOf<ABaseDungeonRoom> RoomClass =
			PickWeightedRoomClass(RoomRules, RoomClassCounts, RandomStream);

		if (!RoomClass)
		{
			break;
		}

		const int32 RotationSteps = RandomStream.RandRange(0, 3);
		const bool bPlaced = PlacementSystem.TryPlaceRoomAtDoor(
			SelectedOpenDoor,
			RoomClass,
			RotationSteps
		);

		if (bPlaced)
		{
			int32& Count = RoomClassCounts.FindOrAdd(RoomClass);
			++Count;
		}
	}
}

bool FRandomGrowthAlgorithm::TryPlaceSpecificRoomClass(
	FRoomPlacementSystem& PlacementSystem,
	TSubclassOf<ABaseDungeonRoom> RoomClass,
	FRandomStream& RandomStream
) const
{
	if (!RoomClass)
	{
		return false;
	}

	TArray<FOpenDungeonDoor> OpenDoors = PlacementSystem.GetOpenDoors();

	while (OpenDoors.Num() > 0)
	{
		const int32 DoorIndex = RandomStream.RandRange(0, OpenDoors.Num() - 1);
		const FOpenDungeonDoor SelectedDoor = OpenDoors[DoorIndex];
		OpenDoors.RemoveAt(DoorIndex);

		TArray<int32> Rotations;
		Rotations.Add(0);
		Rotations.Add(1);
		Rotations.Add(2);
		Rotations.Add(3);

		while (Rotations.Num() > 0)
		{
			const int32 RotationIndex = RandomStream.RandRange(0, Rotations.Num() - 1);
			const int32 RotationSteps = Rotations[RotationIndex];
			Rotations.RemoveAt(RotationIndex);

			if (PlacementSystem.TryPlaceRoomAtDoor(SelectedDoor, RoomClass, RotationSteps))
			{
				return true;
			}
		}
	}

	return false;
}

TSubclassOf<ABaseDungeonRoom> FRandomGrowthAlgorithm::PickWeightedRoomClass(
	const TArray<FRoomSpawnRule>& RoomRules,
	const TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts,
	FRandomStream& RandomStream
) const
{
	TArray<const FRoomSpawnRule*> CandidateRules;

	for (const FRoomSpawnRule& Rule : RoomRules)
	{
		if (CanUseRoomRule(Rule, RoomClassCounts))
		{
			CandidateRules.Add(&Rule);
		}
	}

	if (CandidateRules.Num() == 0)
	{
		return nullptr;
	}

	int32 TotalWeight = 0;
	for (const FRoomSpawnRule* Rule : CandidateRules)
	{
		TotalWeight += FMath::Max(1, Rule->Weight);
	}

	int32 RandomValue = RandomStream.RandRange(1, TotalWeight);
	for (const FRoomSpawnRule* Rule : CandidateRules)
	{
		RandomValue -= FMath::Max(1, Rule->Weight);
		if (RandomValue <= 0)
		{
			return Rule->RoomClass;
		}
	}

	return CandidateRules[0]->RoomClass;
}

bool FRandomGrowthAlgorithm::CanUseRoomRule(
	const FRoomSpawnRule& Rule,
	const TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts
) const
{
	if (!Rule.RoomClass || Rule.Weight <= 0)
	{
		return false;
	}

	const int32 CurrentCount = GetRoomCount(Rule.RoomClass, RoomClassCounts);
	if (Rule.MaxCount >= 0 && CurrentCount >= Rule.MaxCount)
	{
		return false;
	}

	return true;
}

int32 FRandomGrowthAlgorithm::GetRoomCount(
	TSubclassOf<ABaseDungeonRoom> RoomClass,
	const TMap<TSubclassOf<ABaseDungeonRoom>, int32>& RoomClassCounts
) const
{
	const int32* CountPtr = RoomClassCounts.Find(RoomClass);
	return CountPtr ? *CountPtr : 0;
}

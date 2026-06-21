#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseDungeonRoom.h"
#include "RoomPlacementSystem.h"
#include "DungeonGenerator.generated.h"

USTRUCT(BlueprintType)
struct FRoomSpawnRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Rule")
	TSubclassOf<ABaseDungeonRoom> RoomClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Rule")
	int32 MinCount = 0;

	// -1 means unlimited.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Rule")
	int32 MaxCount = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Rule")
	int32 Weight = 1;

	/**
	 * When enabled, the generator places the required MinCount copies of this
	 * room before normal random growth, then connects all key rooms via MST.
	 * For a base / boss / extraction room, enable this and normally set MaxCount
	 * equal to MinCount when you do not want extra random copies later.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Rule|Key Layout")
	bool bPreplaceAsKeyRoom = false;
};

UCLASS()
class RANDOMDUNGEON_API ADungeonGenerator : public AActor
{
	GENERATED_BODY()

public:
	ADungeonGenerator();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Location")
	FVector DungeonOrigin = FVector(0.f, 0.f, -5000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rooms")
	TSubclassOf<ABaseDungeonRoom> StartRoomClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rooms")
	TArray<FRoomSpawnRule> RoomRules;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Total Rooms")
	int32 TotalMinRooms = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Total Rooms")
	int32 TotalMaxRooms = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rules")
	int32 SafetyLimit = 100000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rules")
	int32 GenerationRetryCount = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rules")
	FVector CellSize = FVector(2000.f, 2000.f, 2000.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rules")
	int32 Seed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Generation")
	bool bUseRandomSeed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Rules")
	bool bGenerateOnBeginPlay = false;

	// ---------------- Key room preplacement + MST ----------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout")
	bool bUseKeyRoomMSTLayout = false;

	/**
	 * Required when bUseKeyRoomMSTLayout is enabled and more than one key room exists.
	 * Create a one-cell connector blueprint with valid DoorSlots in all six directions.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (EditCondition = "bUseKeyRoomMSTLayout"))
	TSubclassOf<ABaseDungeonRoom> ConnectorRoomClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "1", EditCondition = "bUseKeyRoomMSTLayout"))
	int32 KeyRoomPlacementRadiusXY = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "0", EditCondition = "bUseKeyRoomMSTLayout"))
	int32 KeyRoomMinXYDistance = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "0", EditCondition = "bUseKeyRoomMSTLayout"))
	int32 MaxAbsKeyRoomZ = 3;

	/**
	 * Every extra Z layer is sampled with this continuation probability.
	 * Example 0.4: after choosing a direction, each next layer continues with 40% chance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "0.0", ClampMax = "0.99", EditCondition = "bUseKeyRoomMSTLayout"))
	float KeyRoomVerticalContinueProbability = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (EditCondition = "bUseKeyRoomMSTLayout"))
	int32 MinGridZ = -3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (EditCondition = "bUseKeyRoomMSTLayout"))
	int32 MaxGridZ = 3;

	/** Higher values make A* prefer horizontal links over stairs / vertical links. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "1.0", EditCondition = "bUseKeyRoomMSTLayout"))
	float VerticalRouteCost = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "1", EditCondition = "bUseKeyRoomMSTLayout"))
	int32 RouteSearchPadding = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "128", EditCondition = "bUseKeyRoomMSTLayout"))
	int32 MaxRouteSearchNodes = 8192;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "1", EditCondition = "bUseKeyRoomMSTLayout"))
	int32 KeyRoomPlacementAttempts = 200;

	/**
	 * Ensures TotalMinRooms is not chosen below the expected connector-room budget.
	 * Increase this if key rooms are placed farther apart or if routes often fail for budget reasons.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon|Key Room Layout", meta = (ClampMin = "0", EditCondition = "bUseKeyRoomMSTLayout"))
	int32 MinimumConnectorRoomBudget = 8;

	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void GenerateDungeon();

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	ABaseDungeonRoom* StartRoomActor = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	FTransform GetDungeonEntryTransform() const;

private:
	FRandomStream RandomStream;
	FRoomPlacementSystem PlacementSystem;

	int32 GetTargetRoomCount();
	int32 GetRequiredMinimumRoomCount() const;
	int32 GetKeyRoomNodeCount() const;
	int32 GetMinimumKeyRoomConnectorBudget() const;
	bool GenerateDungeonOnce(int32 AttemptIndex);
};

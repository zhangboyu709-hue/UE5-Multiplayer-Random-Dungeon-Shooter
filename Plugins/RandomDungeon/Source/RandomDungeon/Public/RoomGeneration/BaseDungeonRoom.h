#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseDungeonRoom.generated.h"

USTRUCT(BlueprintType)
struct FRoomDoor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Door")
	FIntVector Cell = FIntVector::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Door")
	int32 Direction = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Room Door")
	FName DoorName;
};

UCLASS()
class RANDOMDUNGEON_API ABaseDungeonRoom : public AActor
{
	GENERATED_BODY()

public:
	ABaseDungeonRoom();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Room|Shape")
	TArray<FIntVector> Cells;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon Room|Doors")
	TArray<FRoomDoor> Doors;

	UPROPERTY(ReplicatedUsing = OnRep_OpenedDoors)
	TArray<FName> OpenedDoors;

	UFUNCTION(BlueprintCallable, Category = "Dungeon Room|Spawn")
	USceneComponent* GetPlayerSpawnPoint() const;

	UFUNCTION(BlueprintCallable, Category = "Dungeon Room|Doors")
	void OpenDoor(const FRoomDoor& Door);

	UFUNCTION()
	void OnRep_OpenedDoors();

	void ApplyOpenDoorVisual(FName DoorName);

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaTime) override;
};
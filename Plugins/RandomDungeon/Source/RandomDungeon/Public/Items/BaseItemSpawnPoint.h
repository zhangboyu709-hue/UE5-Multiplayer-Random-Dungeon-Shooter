#pragma once
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseItemSpawnPoint.generated.h"

USTRUCT(BlueprintType)
struct FItemSpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	TSubclassOf<AActor> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	int32 Weight = 1;
};

UCLASS()
class RANDOMDUNGEON_API ABaseItemSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ABaseItemSpawnPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	TArray<FItemSpawnEntry> ItemPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	bool bAllowInitialSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Spawn")
	float SpawnRadius = 50.f;

	UFUNCTION(BlueprintCallable, Category = "Item Spawn")
	TSubclassOf<AActor> PickItemClass() const;

	UFUNCTION(BlueprintCallable, Category = "Item Spawn")
	AActor* SpawnItem(TSubclassOf<AActor> ItemClass);

protected:
	FVector GetRandomSpawnLocation() const;
};
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseSpawnPoint.generated.h"

USTRUCT(BlueprintType)
struct FEnemySpawnEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TSubclassOf<AActor> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 Weight = 1;
};

UCLASS()
class RANDOMDUNGEON_API ABaseSpawnPoint : public AActor
{
	GENERATED_BODY()

public:
	ABaseSpawnPoint();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	TArray<FEnemySpawnEntry> EnemyPool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	float SpawnRadius = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bAllowInitialSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	bool bAllowDynamicSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn Point")
	int32 MaxDynamicSpawnCount = -1;

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	bool CanDynamicSpawn() const;

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	AActor* SpawnEnemy(TSubclassOf<AActor> EnemyClass);

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	TSubclassOf<AActor> PickEnemyClass() const;

	UFUNCTION(BlueprintCallable, Category = "Spawn Point")
	void NotifyDynamicSpawned();

protected:
	UPROPERTY()
	int32 DynamicSpawnedCount = 0;

	FVector GetRandomSpawnLocation() const;
};
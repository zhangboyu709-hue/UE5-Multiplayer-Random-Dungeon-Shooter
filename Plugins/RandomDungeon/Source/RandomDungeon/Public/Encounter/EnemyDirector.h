#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyDirector.generated.h"

class ABaseSpawnPoint;

USTRUCT(BlueprintType)
struct FEnemyLimitRule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MaxAlive = -1;
};

UCLASS()
class RANDOMDUNGEON_API AEnemyDirector : public AActor
{
	GENERATED_BODY()

public:
	AEnemyDirector();

protected:
	virtual void BeginPlay() override;

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 InitialSpawnTotal = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float DynamicSpawnInterval = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 DynamicSpawnPerTick = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	int32 MaxAliveEnemies = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	float MinSpawnDistanceToPlayer = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Spawn")
	TArray<FEnemyLimitRule> EnemyLimitRules;

	UFUNCTION(BlueprintCallable)
	void StartDirector();

	UFUNCTION(BlueprintCallable)
	void CollectSpawnPoints();

	UFUNCTION(BlueprintCallable)
	void SpawnInitialEnemies();

	UFUNCTION(BlueprintCallable)
	void TryDynamicSpawn();

protected:

	UPROPERTY()
	TArray<ABaseSpawnPoint*> SpawnPoints;

	UPROPERTY()
	TArray<AActor*> AliveEnemies;

	FTimerHandle SpawnTimer;

	void CleanupDeadEnemies();

	bool CanSpawnClass(TSubclassOf<AActor> EnemyClass);

	bool IsFarEnoughFromPlayers(
		const FVector& SpawnLocation
	) const;

	ABaseSpawnPoint* PickRandomSpawnPoint(
		bool bDynamic
	);

	AActor* SpawnFromPoint(
		ABaseSpawnPoint* SpawnPoint,
		bool bDynamic
	);
};
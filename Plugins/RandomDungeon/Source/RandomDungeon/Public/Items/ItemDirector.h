#pragma once
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ItemDirector.generated.h"

class ABaseItemSpawnPoint;

UCLASS()
class RANDOMDUNGEON_API AItemDirector : public AActor
{
	GENERATED_BODY()

public:
	AItemDirector();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item Director")
	int32 InitialItemTotal = 10;

	UFUNCTION(BlueprintCallable, Category = "Item Director")
	void StartDirector();

	UFUNCTION(BlueprintCallable, Category = "Item Director")
	void CollectSpawnPoints();

	UFUNCTION(BlueprintCallable, Category = "Item Director")
	void SpawnInitialItems();

protected:
	UPROPERTY()
	TArray<ABaseItemSpawnPoint*> SpawnPoints;

	UPROPERTY()
	TArray<AActor*> SpawnedItems;

	ABaseItemSpawnPoint* PickRandomSpawnPoint() const;
};
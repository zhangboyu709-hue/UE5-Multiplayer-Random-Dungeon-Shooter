#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonRuntimeManager.generated.h"

class ADungeonGenerator;
class AEnemyDirector;
class AItemDirector;

UCLASS()
class RANDOMDUNGEON_API ADungeonRuntimeManager : public AActor
{
	GENERATED_BODY()

public:
	ADungeonRuntimeManager();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime|Classes")
	TSubclassOf<ADungeonGenerator> DungeonGeneratorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime|Classes")
	TSubclassOf<AEnemyDirector> EnemyDirectorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime|Classes")
	TSubclassOf<AItemDirector> ItemDirectorClass;

	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	ADungeonGenerator* DungeonGenerator = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	AEnemyDirector* EnemyDirector = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Runtime")
	AItemDirector* ItemDirector = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	bool bStartOnBeginPlay = true;

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void StartRuntime();

private:
	void SpawnRuntimeActors();
};
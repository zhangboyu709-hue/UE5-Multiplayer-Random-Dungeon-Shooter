#include "DungeonRuntimeManager.h"
#include "RoomGeneration/DungeonGenerator.h"
#include "Encounter/EnemyDirector.h"
#include "Items/ItemDirector.h"
#include "Engine/World.h"

ADungeonRuntimeManager::ADungeonRuntimeManager()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void ADungeonRuntimeManager::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (bStartOnBeginPlay)
	{
		StartRuntime();
	}
}

void ADungeonRuntimeManager::StartRuntime()
{
	UE_LOG(LogTemp, Warning, TEXT("RuntimeManager StartRuntime"));
	if (!HasAuthority())
	{
		return;
	}

	SpawnRuntimeActors();

	if (DungeonGenerator)
	{
		DungeonGenerator->GenerateDungeon();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RuntimeManager: DungeonGenerator missing."));
		return;
	}

	if (EnemyDirector)
	{
		EnemyDirector->StartDirector();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RuntimeManager: EnemyDirector missing."));
	}
	UE_LOG(LogTemp, Warning, TEXT("ItemDirector StartDirector"));
	if (ItemDirector)
	{
		ItemDirector->StartDirector();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("RuntimeManager: ItemDirector missing."));
	}
}

void ADungeonRuntimeManager::SpawnRuntimeActors()
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return;
	}

	if (!DungeonGenerator && DungeonGeneratorClass)
	{
		DungeonGenerator = World->SpawnActor<ADungeonGenerator>(
			DungeonGeneratorClass,
			GetActorLocation(),
			GetActorRotation()
		);

		if (DungeonGenerator)
		{
			DungeonGenerator->bGenerateOnBeginPlay = false;
		}
	}

	if (!EnemyDirector && EnemyDirectorClass)
	{
		EnemyDirector = World->SpawnActor<AEnemyDirector>(
			EnemyDirectorClass,
			GetActorLocation(),
			GetActorRotation()
		);
	}

	if (!ItemDirector && ItemDirectorClass)
	{
		ItemDirector = World->SpawnActor<AItemDirector>(
			ItemDirectorClass,
			GetActorLocation(),
			GetActorRotation()
		);
	}
}
#include "Encounter/BaseSpawnPoint.h"
#include "Engine/World.h"

ABaseSpawnPoint::ABaseSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

bool ABaseSpawnPoint::CanDynamicSpawn() const
{
	if (!bAllowDynamicSpawn)
	{
		return false;
	}

	if (MaxDynamicSpawnCount >= 0 && DynamicSpawnedCount >= MaxDynamicSpawnCount)
	{
		return false;
	}

	return true;
}

TSubclassOf<AActor> ABaseSpawnPoint::PickEnemyClass() const
{
	int32 TotalWeight = 0;

	for (const FEnemySpawnEntry& Entry : EnemyPool)
	{
		if (Entry.EnemyClass && Entry.Weight > 0)
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= 0)
	{
		return nullptr;
	}

	int32 RandomValue = FMath::RandRange(1, TotalWeight);

	for (const FEnemySpawnEntry& Entry : EnemyPool)
	{
		if (!Entry.EnemyClass || Entry.Weight <= 0)
		{
			continue;
		}

		RandomValue -= Entry.Weight;

		if (RandomValue <= 0)
		{
			return Entry.EnemyClass;
		}
	}

	return nullptr;
}

FVector ABaseSpawnPoint::GetRandomSpawnLocation() const
{
	return GetActorLocation() + FVector(
		FMath::RandRange(-SpawnRadius, SpawnRadius),
		FMath::RandRange(-SpawnRadius, SpawnRadius),
		0.f
	);
}

AActor* ABaseSpawnPoint::SpawnEnemy(TSubclassOf<AActor> EnemyClass)
{
	if (!HasAuthority() || !EnemyClass)
	{
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	return GetWorld()->SpawnActor<AActor>(
		EnemyClass,
		GetRandomSpawnLocation(),
		GetActorRotation(),
		Params
	);
}

void ABaseSpawnPoint::NotifyDynamicSpawned()
{
	DynamicSpawnedCount++;
}
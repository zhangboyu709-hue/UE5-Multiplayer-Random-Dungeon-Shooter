#include "Items/BaseItemSpawnPoint.h"
#include "Engine/World.h"

ABaseItemSpawnPoint::ABaseItemSpawnPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

TSubclassOf<AActor> ABaseItemSpawnPoint::PickItemClass() const
{
	int32 TotalWeight = 0;

	for (const FItemSpawnEntry& Entry : ItemPool)
	{
		if (Entry.ItemClass && Entry.Weight > 0)
		{
			TotalWeight += Entry.Weight;
		}
	}

	if (TotalWeight <= 0)
	{
		return nullptr;
	}

	int32 RandomValue = FMath::RandRange(1, TotalWeight);

	for (const FItemSpawnEntry& Entry : ItemPool)
	{
		if (!Entry.ItemClass || Entry.Weight <= 0)
		{
			continue;
		}

		RandomValue -= Entry.Weight;

		if (RandomValue <= 0)
		{
			return Entry.ItemClass;
		}
	}

	return nullptr;
}

FVector ABaseItemSpawnPoint::GetRandomSpawnLocation() const
{
	return GetActorLocation() + FVector(
		FMath::RandRange(-SpawnRadius, SpawnRadius),
		FMath::RandRange(-SpawnRadius, SpawnRadius),
		0.f
	);
}

AActor* ABaseItemSpawnPoint::SpawnItem(TSubclassOf<AActor> ItemClass)
{
	if (!HasAuthority() || !ItemClass)
	{
		UE_LOG(LogTemp, Error, TEXT("ItemSpawnPoint: Spawn failed. HasAuthority=%d ItemClass=%s"),
			HasAuthority(),
			*GetNameSafe(ItemClass)
		);
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector SpawnLocation = GetRandomSpawnLocation();

	AActor* SpawnedItem = GetWorld()->SpawnActor<AActor>(
		ItemClass,
		SpawnLocation,
		GetActorRotation(),
		Params
	);

	if (SpawnedItem)
	{
		UE_LOG(LogTemp, Warning, TEXT("ItemSpawnPoint: Spawned %s at %s"),
			*GetNameSafe(SpawnedItem),
			*SpawnLocation.ToString()
		);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("ItemSpawnPoint: SpawnActor returned nullptr. ItemClass=%s Location=%s"),
			*GetNameSafe(ItemClass),
			*SpawnLocation.ToString()
		);
	}

	return SpawnedItem;
}
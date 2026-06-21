#include "Items/ItemDirector.h"
#include "Items/BaseItemSpawnPoint.h"
#include "Kismet/GameplayStatics.h"

AItemDirector::AItemDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AItemDirector::BeginPlay()
{
	Super::BeginPlay();
}

void AItemDirector::StartDirector()
{
	if (!HasAuthority())
	{
		return;
	}

	CollectSpawnPoints();
	SpawnInitialItems();
}

void AItemDirector::CollectSpawnPoints()
{
	SpawnPoints.Empty();

	TArray<AActor*> Found;

	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		ABaseItemSpawnPoint::StaticClass(),
		Found
	);

	for (AActor* Actor : Found)
	{
		if (ABaseItemSpawnPoint* Point = Cast<ABaseItemSpawnPoint>(Actor))
		{
			SpawnPoints.Add(Point);
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("ItemDirector: Collected %d item spawn points."), SpawnPoints.Num());
}

void AItemDirector::SpawnInitialItems()
{
	UE_LOG(LogTemp, Warning, TEXT("ItemDirector: Try spawn %d items."), InitialItemTotal);
	for (int32 i = 0; i < InitialItemTotal; i++)
	{
		ABaseItemSpawnPoint* Point = PickRandomSpawnPoint();

		if (!Point)
		{
			return;
		}

		TSubclassOf<AActor> ItemClass = Point->PickItemClass();

		if (!ItemClass)
		{
			continue;
		}

		AActor* Item = Point->SpawnItem(ItemClass);

		if (Item)
		{
			SpawnedItems.Add(Item);
		}
	}
}

ABaseItemSpawnPoint* AItemDirector::PickRandomSpawnPoint() const
{
	TArray<ABaseItemSpawnPoint*> Candidates;

	for (ABaseItemSpawnPoint* Point : SpawnPoints)
	{
		if (Point && Point->bAllowInitialSpawn)
		{
			Candidates.Add(Point);
		}
	}

	if (Candidates.Num() == 0)
	{
		return nullptr;
	}

	return Candidates[FMath::RandRange(0, Candidates.Num() - 1)];
}
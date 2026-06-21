#include "Encounter/EnemyDirector.h"
#include "Encounter/BaseSpawnPoint.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

AEnemyDirector::AEnemyDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AEnemyDirector::BeginPlay()
{
	Super::BeginPlay();
}

void AEnemyDirector::StartDirector()
{
	if (!HasAuthority())
	{
		return;
	}

	CollectSpawnPoints();

	SpawnInitialEnemies();

	GetWorldTimerManager().SetTimer(
		SpawnTimer,
		this,
		&AEnemyDirector::TryDynamicSpawn,
		DynamicSpawnInterval,
		true
	);
}

void AEnemyDirector::CollectSpawnPoints()
{
	SpawnPoints.Empty();

	TArray<AActor*> Found;

	UGameplayStatics::GetAllActorsOfClass(
		GetWorld(),
		ABaseSpawnPoint::StaticClass(),
		Found
	);

	for (AActor* Actor : Found)
	{
		if (ABaseSpawnPoint* SP =
			Cast<ABaseSpawnPoint>(Actor))
		{
			SpawnPoints.Add(SP);
		}
	}
}

void AEnemyDirector::SpawnInitialEnemies()
{
	for (int32 i = 0; i < InitialSpawnTotal; i++)
	{
		ABaseSpawnPoint* Point =
			PickRandomSpawnPoint(false);

		if (!Point)
		{
			return;
		}

		SpawnFromPoint(Point, false);
	}
}

void AEnemyDirector::TryDynamicSpawn()
{
	CleanupDeadEnemies();

	if (AliveEnemies.Num() >= MaxAliveEnemies)
	{
		return;
	}

	for (int32 i = 0; i < DynamicSpawnPerTick; i++)
	{
		if (AliveEnemies.Num() >= MaxAliveEnemies)
		{
			return;
		}

		ABaseSpawnPoint* Point =
			PickRandomSpawnPoint(true);

		if (!Point)
		{
			return;
		}

		SpawnFromPoint(Point, true);
	}
}

void AEnemyDirector::CleanupDeadEnemies()
{
	for (int32 i = AliveEnemies.Num() - 1; i >= 0; i--)
	{
		if (!IsValid(AliveEnemies[i]))
		{
			AliveEnemies.RemoveAt(i);
		}
	}
}

bool AEnemyDirector::IsFarEnoughFromPlayers(
	const FVector& SpawnLocation
) const
{
	UWorld* World = GetWorld();

	if (!World)
	{
		return false;
	}

	for (FConstPlayerControllerIterator It =
		World->GetPlayerControllerIterator();
		It;
		++It)
	{
		APlayerController* PC = It->Get();

		if (!PC)
		{
			continue;
		}

		APawn* Pawn = PC->GetPawn();

		if (!Pawn)
		{
			continue;
		}

		if (
			FVector::Dist(
				SpawnLocation,
				Pawn->GetActorLocation()
			)
			< MinSpawnDistanceToPlayer
			)
		{
			return false;
		}
	}

	return true;
}

bool AEnemyDirector::CanSpawnClass(
	TSubclassOf<AActor> EnemyClass
)
{
	for (const FEnemyLimitRule& Rule :
		EnemyLimitRules)
	{
		if (
			Rule.EnemyClass != EnemyClass
			||
			Rule.MaxAlive < 0
			)
		{
			continue;
		}

		int32 Count = 0;

		for (AActor* Enemy : AliveEnemies)
		{
			if (
				IsValid(Enemy)
				&&
				Enemy->IsA(EnemyClass)
				)
			{
				Count++;
			}
		}

		if (Count >= Rule.MaxAlive)
		{
			return false;
		}
	}

	return true;
}

ABaseSpawnPoint*
AEnemyDirector::PickRandomSpawnPoint(
	bool bDynamic
)
{
	TArray<ABaseSpawnPoint*> Candidates;

	for (ABaseSpawnPoint* Point :
		SpawnPoints)
	{
		if (!Point)
		{
			continue;
		}

		if (
			bDynamic
			&&
			!Point->CanDynamicSpawn()
			)
		{
			continue;
		}

		if (
			!IsFarEnoughFromPlayers(
				Point->GetActorLocation()
			)
			)
		{
			continue;
		}

		Candidates.Add(Point);
	}

	if (Candidates.IsEmpty())
	{
		return nullptr;
	}

	return Candidates[
		FMath::RandRange(
			0,
			Candidates.Num() - 1
		)
	];
}

AActor* AEnemyDirector::SpawnFromPoint(
	ABaseSpawnPoint* SpawnPoint,
	bool bDynamic
)
{
	if (!SpawnPoint)
	{
		return nullptr;
	}

	TSubclassOf<AActor> EnemyClass =
		SpawnPoint->PickEnemyClass();

	if (!CanSpawnClass(EnemyClass))
	{
		return nullptr;
	}

	AActor* Enemy =
		SpawnPoint->SpawnEnemy(
			EnemyClass
		);

	if (Enemy)
	{
		AliveEnemies.Add(Enemy);

		if (bDynamic)
		{
			SpawnPoint
				->NotifyDynamicSpawned();
		}
	}

	return Enemy;
}
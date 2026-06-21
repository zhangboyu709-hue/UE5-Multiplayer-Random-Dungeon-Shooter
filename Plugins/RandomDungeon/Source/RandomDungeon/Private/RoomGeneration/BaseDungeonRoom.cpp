#include "RoomGeneration/BaseDungeonRoom.h"
#include "Net/UnrealNetwork.h"
#include "Components/PrimitiveComponent.h"

ABaseDungeonRoom::ABaseDungeonRoom()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Cells.Add(FIntVector(0, 0, 0));
}

void ABaseDungeonRoom::BeginPlay()
{
	Super::BeginPlay();
}

void ABaseDungeonRoom::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

USceneComponent* ABaseDungeonRoom::GetPlayerSpawnPoint() const
{
	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);

	for (USceneComponent* Comp : Components)
	{
		if (Comp && Comp->GetName() == TEXT("PlayerSpawn"))
		{
			return Comp;
		}
	}

	return nullptr;
}

void ABaseDungeonRoom::OpenDoor(const FRoomDoor& Door)
{
	if (Door.DoorName.IsNone())
	{
		return;
	}

	if (HasAuthority())
	{
		OpenedDoors.AddUnique(Door.DoorName);
	}

	ApplyOpenDoorVisual(Door.DoorName);
}

void ABaseDungeonRoom::OnRep_OpenedDoors()
{
	for (const FName& DoorName : OpenedDoors)
	{
		ApplyOpenDoorVisual(DoorName);
	}
}

void ABaseDungeonRoom::ApplyOpenDoorVisual(FName DoorName)
{
	const FString DoorObjectName = DoorName.ToString();

	TArray<USceneComponent*> Components;
	GetComponents<USceneComponent>(Components);

	for (USceneComponent* Comp : Components)
	{
		if (Comp && Comp->GetName() == DoorObjectName)
		{
			Comp->SetHiddenInGame(true, true);

			if (UPrimitiveComponent* Prim = Cast<UPrimitiveComponent>(Comp))
			{
				Prim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}

			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("OpenDoor failed: door object not found: %s"), *DoorObjectName);
}

void ABaseDungeonRoom::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseDungeonRoom, OpenedDoors);
}
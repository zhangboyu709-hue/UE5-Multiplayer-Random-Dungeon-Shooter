#include "Inventory/InventoryComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

UInventoryComponent::UInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UInventoryComponent, Items);
}

bool UInventoryComponent::AddPickupItem(ABasePickupItem* PickupItem)
{
	if (!PickupItem)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return false;
	}

	if (!PickupItem->bCanPickup)
	{
		return false;
	}

	TSubclassOf<ABasePickupItem> NewItemClass = PickupItem->GetClass();
	const int32 NewItemValue = PickupItem->GetItemValue();

	for (FCarriedItem& Item : Items)
	{
		if (Item.ItemClass == NewItemClass && Item.ItemValue == NewItemValue)
		{
			Item.Count++;
			PickupItem->Destroy();
			return true;
		}
	}

	FCarriedItem NewItem;
	NewItem.ItemClass = NewItemClass;
	NewItem.ItemValue = NewItemValue;
	NewItem.Count = 1;

	Items.Add(NewItem);

	PickupItem->Destroy();
	return true;
}


void UInventoryComponent::DropAllItems(FVector DropCenter, float ScatterRadius)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();

	if (!OwnerActor || !OwnerActor->HasAuthority() || !World)
	{
		return;
	}

	ScatterRadius = FMath::Max(ScatterRadius, 30.f);

	TArray<FCarriedItem> RemainingItems;

	for (const FCarriedItem& CarriedItem : Items)
	{
		if (!CarriedItem.ItemClass || CarriedItem.Count <= 0)
		{
			continue;
		}

		int32 SpawnedCount = 0;

		for (int32 Index = 0; Index < CarriedItem.Count; ++Index)
		{
			const float Angle = FMath::FRandRange(0.f, 2.f * PI);
			const float Distance = FMath::FRandRange(30.f, ScatterRadius);

			const FVector Offset(
				FMath::Cos(Angle) * Distance,
				FMath::Sin(Angle) * Distance,
				FMath::FRandRange(5.f, 25.f)
			);

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			ABasePickupItem* DroppedItem = World->SpawnActor<ABasePickupItem>(
				CarriedItem.ItemClass,
				DropCenter + Offset,
				FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f),
				SpawnParams
			);

			if (DroppedItem)
			{
				DroppedItem->ItemValue = CarriedItem.ItemValue;
				DroppedItem->bCanPickup = true;
				SpawnedCount++;
			}
		}

		if (SpawnedCount < CarriedItem.Count)
		{
			FCarriedItem Remaining = CarriedItem;
			Remaining.Count -= SpawnedCount;
			RemainingItems.Add(Remaining);
		}
	}

	Items = MoveTemp(RemainingItems);
}

void UInventoryComponent::ClearInventory()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	Items.Empty();
}

int32 UInventoryComponent::GetTotalValue() const
{
	int32 Total = 0;

	for (const FCarriedItem& Item : Items)
	{
		Total += Item.ItemValue * Item.Count;
	}

	return Total;
}
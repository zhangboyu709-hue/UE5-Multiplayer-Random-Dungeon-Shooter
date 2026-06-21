#include "Items/BasePickupItem.h"
#include "Components/StaticMeshComponent.h"

ABasePickupItem::ABasePickupItem()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	RootComponent = ItemMesh;

	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);

	Tags.Add(TEXT("Pickup"));
}

int32 ABasePickupItem::GetItemValue() const
{
	return ItemValue;
}

FName ABasePickupItem::GetItemName() const
{
	return ItemName;
}
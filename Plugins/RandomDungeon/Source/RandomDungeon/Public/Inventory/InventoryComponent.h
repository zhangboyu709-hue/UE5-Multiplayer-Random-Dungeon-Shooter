#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Items/BasePickupItem.h"
#include "InventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct RANDOMDUNGEON_API FCarriedItem
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	TSubclassOf<ABasePickupItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 ItemValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Count = 1;
};

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class RANDOMDUNGEON_API UInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UInventoryComponent();

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
	TArray<FCarriedItem> Items;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	bool AddPickupItem(ABasePickupItem* PickupItem);

	// 仅服务器执行：将背包中的每件物品重新生成在指定点周围。
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void DropAllItems(FVector DropCenter, float ScatterRadius = 180.f);

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	void ClearInventory();

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	int32 GetTotalValue() const;
};

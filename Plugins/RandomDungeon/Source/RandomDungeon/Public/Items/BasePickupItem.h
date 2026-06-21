#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BasePickupItem.generated.h"

UCLASS()
class RANDOMDUNGEON_API ABasePickupItem : public AActor
{
	GENERATED_BODY()

public:
	ABasePickupItem();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Item")
	UStaticMeshComponent* ItemMesh;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	FName ItemName = TEXT("Item");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	int32 ItemValue = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Item")
	bool bCanPickup = true;

	UFUNCTION(BlueprintCallable, Category = "Item")
	int32 GetItemValue() const;

	UFUNCTION(BlueprintCallable, Category = "Item")
	FName GetItemName() const;
};

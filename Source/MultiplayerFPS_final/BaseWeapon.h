#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseWeapon.generated.h"

class USkeletalMeshComponent;
class USoundBase;
class UNiagaraSystem;
class ADungeonFPSCharacterBase;
class ABaseProjectile;

UCLASS()
class MULTIPLAYERFPS_FINAL_API ABaseWeapon : public AActor
{
	GENERATED_BODY()

public:
	ABaseWeapon();

	// 只应由服务器调用。角色输入会通过 ServerFire 转到服务器。
	virtual void Fire(ADungeonFPSCharacterBase* OwnerCharacter);

	UFUNCTION(BlueprintPure, Category = "Weapon")
	USkeletalMeshComponent* GetWeaponMesh() const { return WeaponMesh; }

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FTransform GetSocketWorldTransform(FName SocketName) const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FTransform GetMuzzleWorldTransform() const;

	UFUNCTION(BlueprintPure, Category = "Weapon")
	FTransform GetLeftHandIKWorldTransform() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void AttachToCharacterMeshAligned(
		USkeletalMeshComponent* CharacterMesh,
		FName CharacterSocketName,
		FName InGripSocketName
	);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* WeaponMesh;

protected:
	// ===== Projectile Config =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	TSubclassOf<ABaseProjectile> ProjectileClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	float Damage = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	float ProjectileSpeed = 6000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	float ProjectileLifeSeconds = 5.0f;

	// 普通枪 = 1；霰弹枪 = 8/10/12。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	int32 PelletCount = 1;

	// 普通枪 = 0；霰弹枪可以 5~8。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	float SpreadAngleDeg = 0.0f;

	// 从屏幕中心找瞄准点的距离。这个射线只算方向，不造成伤害。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Aim")
	float AimRange = 10000.0f;

protected:
	// ===== Sockets =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName MuzzleSocketName = TEXT("MuzzleSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName LeftHandIKSocketName = TEXT("LeftHandIKSocket");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName GripSocketName = TEXT("GripSocket");

protected:
	// ===== FX =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	USoundBase* FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|FX")
	UNiagaraSystem* MuzzleFlash;

protected:
	FVector GetAimDirectionFromOwner(
		ADungeonFPSCharacterBase* OwnerCharacter,
		const FVector& MuzzleLocation
	) const;

	void SpawnProjectiles(ADungeonFPSCharacterBase* OwnerCharacter);

	void PlayFireFX();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayFireFX();
};

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Camera/CameraTypes.h"
#include "DungeonFPSCharacterBase.generated.h"


class UCameraComponent;
class USkeletalMeshComponent;
class USceneComponent;
class UInputMappingContext;
class UInputAction;
class ABaseWeapon;

UCLASS()
class MULTIPLAYERFPS_FINAL_API ADungeonFPSCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	ADungeonFPSCharacterBase();

protected:
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

public:
	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser
	) override;

	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;

	// 服务器调用。用于新回合/特殊复活；普通 Game -> lobby 切图时，
	// 新生成的角色会在 BeginPlay 自动调用它。
	UFUNCTION(BlueprintCallable, Category = "Health")
	void ResetCharacterState();

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsDead() const { return bDead; }

	// ===== Components =====

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UCameraComponent* FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USkeletalMeshComponent* FirstPersonMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* FPWeaponRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USceneComponent* TPWeaponRoot;

	// ===== Input =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputMappingContext* DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* LookAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* JumpAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	UInputAction* FireAction;

	// ===== Health =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Replicated, Category = "Health")
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHealth, Category = "Health")
	float CurrentHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_Dead, Category = "Health")
	bool bDead = false;

	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION()
	void OnRep_Dead();

	void ApplyDeathState();
	void ApplyAliveState();
	void Die(AActor* DamageCauser);

	// 蓝图中可用来更新血条/数字。
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void BP_OnHealthChanged();

	// 服务器死亡时会调用；客户端收到 bDead 复制后也会调用。
	// 后续死亡动画、掉落背包等放这里。掉落类逻辑请在蓝图中先 Switch Has Authority。
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void BP_OnDeath();

	// 死亡角色被复活时调用。普通 Game -> lobby 非无缝切图会生成新角色，
	// 其 BeginPlay 已经自动是满血状态；这个事件主要给以后“原地复活/下一轮重置”用。
	UFUNCTION(BlueprintImplementableEvent, Category = "Health")
	void BP_OnRevived();

	// ===== Weapon =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<ABaseWeapon> StarterWeaponClass;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentWeapon, VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon")
	ABaseWeapon* CurrentWeapon;

	UFUNCTION(BlueprintImplementableEvent, Category = "Weapon")
	void OnWeaponSpawned(ABaseWeapon* NewWeapon);

	UFUNCTION()
	void OnRep_CurrentWeapon();

public:
	UFUNCTION(BlueprintPure, Category = "Weapon")
	ABaseWeapon* GetCurrentWeapon() const { return CurrentWeapon; }

	UFUNCTION(BlueprintPure, Category = "Components")
	UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }

	UFUNCTION(BlueprintPure, Category = "Components")
	USkeletalMeshComponent* GetFirstPersonMesh() const { return FirstPersonMesh; }

protected:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartJump();
	void EndJump();

	void Fire();
	void HandleFire();

	UFUNCTION(Server, Reliable)
	void ServerFire();

	void SpawnStarterWeapon();
};

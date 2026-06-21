#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BaseProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
class USoundBase;
class UNiagaraSystem;

UCLASS()
class MULTIPLAYERFPS_FINAL_API ABaseProjectile : public AActor
{
	GENERATED_BODY()

public:
	ABaseProjectile();

	// 武器生成子弹后调用，用来把武器配置传给子弹。
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitProjectile(
		AController* InInstigatorController,
		AActor* InDamageCauser,
		float InDamage,
		float InInitialSpeed,
		float InLifeSeconds
	);

	// 武器生成子弹后调用，用来设置飞行方向。
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void LaunchInDirection(const FVector& Direction);

protected:
	virtual void BeginPlay() override;

protected:
	// ===== Components =====

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* VisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* ProjectileMovement;

protected:
	// ===== Damage / Lifetime =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	float Damage = 20.0f;

	// 0 表示普通单体伤害；>0 表示爆炸/范围伤害。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	float DamageRadius = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	bool bUseDefaultDamage = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Damage")
	TSubclassOf<UDamageType> DamageTypeClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement")
	float InitialSpeed = 6000.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Movement")
	float LifeSeconds = 5.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	bool bDestroyOnImpact = true;

protected:
	// ===== FX =====

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|FX")
	USoundBase* ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|FX")
	UNiagaraSystem* ImpactFX;

protected:
	UPROPERTY()
	AController* DamageInstigatorController;

	UPROPERTY()
	AActor* DamageCauserActor;

	UPROPERTY()
	bool bHasImpacted = false;

protected:
	UFUNCTION()
	void OnProjectileHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit
	);

	UFUNCTION()
	void OnProjectileOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult
	);

	void ProcessImpact(AActor* OtherActor, const FHitResult& Hit);
	void ApplyDefaultDamage(AActor* OtherActor, const FHitResult& Hit);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayImpactFX(FVector ImpactLocation, FVector ImpactNormal);

	// 服务器命中时调用。燃烧、分裂、生成火焰区域、特殊 Debuff 等额外玩法放这里。
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void BP_OnServerImpact(
		AActor* HitActor,
		FVector ImpactLocation,
		FVector ImpactNormal
	);

	// 所有端播放命中特效时调用。只做视觉/音效，不要在这里扣血。
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void BP_OnImpactFX(
		FVector ImpactLocation,
		FVector ImpactNormal
	);

	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile")
	void BP_OnProjectileLaunched();
};

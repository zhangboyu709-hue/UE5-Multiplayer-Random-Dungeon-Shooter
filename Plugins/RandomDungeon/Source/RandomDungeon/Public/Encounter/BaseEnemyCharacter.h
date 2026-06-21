#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BaseEnemyCharacter.generated.h"

UCLASS()
class RANDOMDUNGEON_API ABaseEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABaseEnemyCharacter();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Replicated, Category = "Enemy|Health")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentHealth, Category = "Enemy|Health")
	float CurrentHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float AttackDamage = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Combat")
	float AttackCooldown = 1.0f;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|State")
	bool bDead = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|Combat")
	bool bCanAttack = false;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "Enemy|Combat")
	bool bHasHitPlayer = false;

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void BeginAttackWindow();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void EndAttackWindow();

	UFUNCTION(BlueprintCallable, Category = "Enemy|Combat")
	void TryMeleeHit(AActor* OtherActor);

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	bool IsDead() const;

	virtual float TakeDamage(
		float DamageAmount,
		struct FDamageEvent const& DamageEvent,
		class AController* EventInstigator,
		AActor* DamageCauser
	) override;

protected:
	UFUNCTION()
	void OnRep_CurrentHealth();

	UFUNCTION(BlueprintCallable, Category = "Enemy")
	void Die(AActor* DamageCauser);

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy")
	void BP_OnHealthChanged();

	UFUNCTION(BlueprintImplementableEvent, Category = "Enemy")
	void BP_OnDeath(AActor* DamageCauser);

public:
	virtual void GetLifetimeReplicatedProps(
		TArray<FLifetimeProperty>& OutLifetimeProps
	) const override;
};
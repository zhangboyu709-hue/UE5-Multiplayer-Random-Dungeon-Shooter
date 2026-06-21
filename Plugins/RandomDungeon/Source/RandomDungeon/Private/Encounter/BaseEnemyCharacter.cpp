#include "Encounter/BaseEnemyCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ABaseEnemyCharacter::ABaseEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	Tags.Add(TEXT("Enemy"));
}

void ABaseEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		CurrentHealth = MaxHealth;
	}
}

float ABaseEnemyCharacter::TakeDamage(
	float DamageAmount,
	FDamageEvent const& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser
)
{
	if (!HasAuthority() || bDead || DamageAmount <= 0.f)
	{
		return 0.f;
	}

	const float ActualDamage = Super::TakeDamage(
		DamageAmount,
		DamageEvent,
		EventInstigator,
		DamageCauser
	);

	CurrentHealth = FMath::Clamp(CurrentHealth - DamageAmount, 0.f, MaxHealth);

	BP_OnHealthChanged();

	if (CurrentHealth <= 0.f)
	{
		Die(DamageCauser);
	}

	return ActualDamage;
}

void ABaseEnemyCharacter::BeginAttackWindow()
{
	if (!HasAuthority() || bDead)
	{
		return;
	}

	bCanAttack = true;
	bHasHitPlayer = false;
}

void ABaseEnemyCharacter::EndAttackWindow()
{
	if (!HasAuthority())
	{
		return;
	}

	bCanAttack = false;
}

void ABaseEnemyCharacter::TryMeleeHit(AActor* OtherActor)
{
	if (!HasAuthority() || bDead || !bCanAttack || bHasHitPlayer || !OtherActor)
	{
		return;
	}

	if (OtherActor == this)
	{
		return;
	}

	UGameplayStatics::ApplyDamage(
		OtherActor,
		AttackDamage,
		GetController(),
		this,
		nullptr
	);

	bHasHitPlayer = true;
}

void ABaseEnemyCharacter::Die(AActor* DamageCauser)
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	bCanAttack = false;

	GetCharacterMovement()->StopMovementImmediately();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BP_OnDeath(DamageCauser);
}

bool ABaseEnemyCharacter::IsDead() const
{
	return bDead;
}

void ABaseEnemyCharacter::OnRep_CurrentHealth()
{
	BP_OnHealthChanged();
}

void ABaseEnemyCharacter::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABaseEnemyCharacter, MaxHealth);
	DOREPLIFETIME(ABaseEnemyCharacter, CurrentHealth);
	DOREPLIFETIME(ABaseEnemyCharacter, bDead);
	DOREPLIFETIME(ABaseEnemyCharacter, bCanAttack);
	DOREPLIFETIME(ABaseEnemyCharacter, bHasHitPlayer);
}
#include "DungeonFPSCharacterBase.h"

#include "BaseWeapon.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

ADungeonFPSCharacterBase::ADungeonFPSCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);

	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 600.0f;

	FirstPersonMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonMesh"));
	FirstPersonMesh->SetupAttachment(GetMesh());
	FirstPersonMesh->SetOnlyOwnerSee(true);
	FirstPersonMesh->SetOwnerNoSee(false);
	FirstPersonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonMesh->bCastDynamicShadow = false;
	FirstPersonMesh->CastShadow = false;

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(FirstPersonMesh, FName("head"));
	FirstPersonCamera->SetRelativeLocationAndRotation(
		FVector(-2.8f, 5.89f, 0.0f),
		FRotator(0.0f, 90.0f, -90.0f)
	);
	FirstPersonCamera->bUsePawnControlRotation = true;

	FPWeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FPWeaponRoot"));
	FPWeaponRoot->SetupAttachment(FirstPersonMesh);

	TPWeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TPWeaponRoot"));
	TPWeaponRoot->SetupAttachment(GetMesh());

	GetMesh()->SetOwnerNoSee(false);
	GetMesh()->SetCastShadow(true);
	GetMesh()->bCastHiddenShadow = true;
}

void ADungeonFPSCharacterBase::CalcCamera(
	float DeltaTime,
	FMinimalViewInfo& OutResult)
{
	Super::CalcCamera(DeltaTime, OutResult);

	// 自己控制自己时，完全保留原本第一人称相机。
	if (IsLocallyControlled())
	{
		return;
	}

	// 别人死亡后切到这个角色观看时：
	// 保留该角色的实际瞄准 Pitch / Yaw，但强制取消 Socket 带来的 Roll 侧翻。
	const FRotator AimRotation = GetBaseAimRotation();

	OutResult.Rotation = FRotator(
		FRotator::NormalizeAxis(AimRotation.Pitch),
		FRotator::NormalizeAxis(AimRotation.Yaw),
		0.0f
	);
}


void ADungeonFPSCharacterBase::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		// 每次新生成的角色（包含 Game -> lobby 后的新角色）都会回到满血、可移动状态。
		ResetCharacterState();
		SpawnStarterWeapon();
	}

	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return;
	}

	ULocalPlayer* LocalPlayer = PC->GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer);

	if (Subsystem && DefaultMappingContext)
	{
		Subsystem->AddMappingContext(DefaultMappingContext, 0);
	}
}

void ADungeonFPSCharacterBase::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EnhancedInput)
	{
		return;
	}

	if (MoveAction)
	{
		EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADungeonFPSCharacterBase::Move);
	}

	if (LookAction)
	{
		EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADungeonFPSCharacterBase::Look);
	}

	if (JumpAction)
	{
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Started, this, &ADungeonFPSCharacterBase::StartJump);
		EnhancedInput->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADungeonFPSCharacterBase::EndJump);
	}

	if (FireAction)
	{
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &ADungeonFPSCharacterBase::Fire);
	}
}

float ADungeonFPSCharacterBase::TakeDamage(
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

	if (ActualDamage <= 0.f)
	{
		return 0.f;
	}

	CurrentHealth = FMath::Clamp(
		CurrentHealth - ActualDamage,
		0.f,
		MaxHealth
	);

	BP_OnHealthChanged();

	if (CurrentHealth <= 0.f)
	{
		Die(DamageCauser);
	}

	ForceNetUpdate();
	return ActualDamage;
}

void ADungeonFPSCharacterBase::ResetCharacterState()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bWasDead = bDead;

	bDead = false;
	CurrentHealth = MaxHealth;

	ApplyAliveState();
	BP_OnHealthChanged();

	if (bWasDead)
	{
		BP_OnRevived();
	}

	ForceNetUpdate();
}

void ADungeonFPSCharacterBase::ApplyDeathState()
{
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADungeonFPSCharacterBase::ApplyAliveState()
{
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetGenerateOverlapEvents(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ADungeonFPSCharacterBase::Die(AActor* DamageCauser)
{
	if (bDead)
	{
		return;
	}

	bDead = true;
	ApplyDeathState();
	BP_OnDeath();
	ForceNetUpdate();
}

void ADungeonFPSCharacterBase::OnRep_CurrentHealth()
{
	BP_OnHealthChanged();
}

void ADungeonFPSCharacterBase::OnRep_Dead()
{
	if (bDead)
	{
		ApplyDeathState();
		BP_OnDeath();
	}
	else
	{
		ApplyAliveState();
		BP_OnRevived();
	}
}

void ADungeonFPSCharacterBase::Move(const FInputActionValue& Value)
{
	if (bDead || !Controller)
	{
		return;
	}

	const FVector2D MoveValue = Value.Get<FVector2D>();

	AddMovementInput(GetActorForwardVector(), MoveValue.Y);
	AddMovementInput(GetActorRightVector(), MoveValue.X);
}

void ADungeonFPSCharacterBase::Look(const FInputActionValue& Value)
{
	const FVector2D LookValue = Value.Get<FVector2D>();

	AddControllerYawInput(LookValue.X);
	AddControllerPitchInput(LookValue.Y);
}

void ADungeonFPSCharacterBase::StartJump()
{
	if (bDead)
	{
		return;
	}

	Jump();
}

void ADungeonFPSCharacterBase::EndJump()
{
	StopJumping();
}

void ADungeonFPSCharacterBase::Fire()
{
	if (bDead)
	{
		return;
	}

	if (HasAuthority())
	{
		HandleFire();
		return;
	}

	ServerFire();
}

void ADungeonFPSCharacterBase::ServerFire_Implementation()
{
	HandleFire();
}

void ADungeonFPSCharacterBase::HandleFire()
{
	if (bDead || !CurrentWeapon)
	{
		return;
	}

	CurrentWeapon->Fire(this);
}

void ADungeonFPSCharacterBase::SpawnStarterWeapon()
{
	if (!HasAuthority() || !StarterWeaponClass || IsValid(CurrentWeapon))
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;

	CurrentWeapon = GetWorld()->SpawnActor<ABaseWeapon>(
		StarterWeaponClass,
		GetActorTransform(),
		SpawnParams
	);

	if (!CurrentWeapon)
	{
		return;
	}

	OnWeaponSpawned(CurrentWeapon);
}

void ADungeonFPSCharacterBase::OnRep_CurrentWeapon()
{
	if (!CurrentWeapon)
	{
		return;
	}

	OnWeaponSpawned(CurrentWeapon);
}

void ADungeonFPSCharacterBase::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADungeonFPSCharacterBase, CurrentWeapon);
	DOREPLIFETIME(ADungeonFPSCharacterBase, MaxHealth);
	DOREPLIFETIME(ADungeonFPSCharacterBase, CurrentHealth);
	DOREPLIFETIME(ADungeonFPSCharacterBase, bDead);
}

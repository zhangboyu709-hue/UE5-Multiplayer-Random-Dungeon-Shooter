#include "BaseWeapon.h"

#include "BaseProjectile.h"
#include "DungeonFPSCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Controller.h"
#include "Engine/World.h"

ABaseWeapon::ABaseWeapon()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);

	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ABaseWeapon::Fire(ADungeonFPSCharacterBase* OwnerCharacter)
{
	if (!OwnerCharacter)
	{
		return;
	}

	// 伤害判定和子弹生成必须只在服务器做。
	if (!HasAuthority())
	{
		return;
	}

	SpawnProjectiles(OwnerCharacter);
	MulticastPlayFireFX();
}

FTransform ABaseWeapon::GetSocketWorldTransform(FName SocketName) const
{
	if (!WeaponMesh || SocketName.IsNone())
	{
		return GetActorTransform();
	}

	if (!WeaponMesh->DoesSocketExist(SocketName))
	{
		return GetActorTransform();
	}

	return WeaponMesh->GetSocketTransform(SocketName, RTS_World);
}

FTransform ABaseWeapon::GetMuzzleWorldTransform() const
{
	return GetSocketWorldTransform(MuzzleSocketName);
}

FTransform ABaseWeapon::GetLeftHandIKWorldTransform() const
{
	return GetSocketWorldTransform(LeftHandIKSocketName);
}

FVector ABaseWeapon::GetAimDirectionFromOwner(
	ADungeonFPSCharacterBase* OwnerCharacter,
	const FVector& MuzzleLocation
) const
{
	AController* OwnerController = OwnerCharacter ? OwnerCharacter->GetController() : nullptr;
	if (!OwnerController)
	{
		return GetActorForwardVector();
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

	const FVector CameraDirection = ViewRotation.Vector();
	const FVector TraceStart = ViewLocation;
	const FVector TraceEnd = TraceStart + CameraDirection * AimRange;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(OwnerCharacter);
	QueryParams.AddIgnoredActor(this);

	FHitResult AimHit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		AimHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams
	);

	const FVector AimPoint = bHit ? AimHit.ImpactPoint : TraceEnd;

	FVector AimDirection = (AimPoint - MuzzleLocation).GetSafeNormal();

	if (AimDirection.IsNearlyZero())
	{
		AimDirection = CameraDirection;
	}

	return AimDirection;
}

void ABaseWeapon::SpawnProjectiles(ADungeonFPSCharacterBase* OwnerCharacter)
{
	if (!ProjectileClass || !OwnerCharacter)
	{
		return;
	}

	AController* OwnerController = OwnerCharacter->GetController();

	const FTransform MuzzleTransform = GetMuzzleWorldTransform();
	const FVector MuzzleLocation = MuzzleTransform.GetLocation();

	const FVector BaseDirection =
		GetAimDirectionFromOwner(OwnerCharacter, MuzzleLocation);

	const int32 SafePelletCount = FMath::Max(1, PelletCount);
	const float SpreadRadians = FMath::DegreesToRadians(FMath::Max(0.0f, SpreadAngleDeg));

	for (int32 i = 0; i < SafePelletCount; i++)
	{
		FVector FireDirection = BaseDirection;

		if (SpreadRadians > 0.0f)
		{
			FireDirection = FMath::VRandCone(BaseDirection, SpreadRadians);
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerCharacter;
		SpawnParams.Instigator = OwnerCharacter;
		SpawnParams.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		ABaseProjectile* Projectile = GetWorld()->SpawnActor<ABaseProjectile>(
			ProjectileClass,
			MuzzleLocation,
			FireDirection.Rotation(),
			SpawnParams
		);

		if (!Projectile)
		{
			continue;
		}

		Projectile->InitProjectile(
			OwnerController,
			this,
			Damage,
			ProjectileSpeed,
			ProjectileLifeSeconds
		);

		Projectile->LaunchInDirection(FireDirection);
	}
}

void ABaseWeapon::PlayFireFX()
{
	if (FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
	}

	if (MuzzleFlash && WeaponMesh)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlash,
			WeaponMesh,
			MuzzleSocketName,
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget,
			true
		);
	}
}

void ABaseWeapon::MulticastPlayFireFX_Implementation()
{
	PlayFireFX();
}

void ABaseWeapon::AttachToCharacterMeshAligned(
	USkeletalMeshComponent* CharacterMesh,
	FName CharacterSocketName,
	FName InGripSocketName
)
{
	if (!CharacterMesh || !WeaponMesh)
	{
		return;
	}

	const FName SocketToUse = InGripSocketName.IsNone() ? GripSocketName : InGripSocketName;

	if (!CharacterMesh->DoesSocketExist(CharacterSocketName))
	{
		UE_LOG(LogTemp, Warning, TEXT("Character socket not found: %s"), *CharacterSocketName.ToString());
		return;
	}

	if (!WeaponMesh->DoesSocketExist(SocketToUse))
	{
		UE_LOG(LogTemp, Warning, TEXT("Weapon grip socket not found: %s"), *SocketToUse.ToString());
		return;
	}

	const FTransform TargetSocketWorld =
		CharacterMesh->GetSocketTransform(CharacterSocketName, RTS_World);

	const FTransform GripSocketWorld =
		WeaponMesh->GetSocketTransform(SocketToUse, RTS_World);

	const FTransform WeaponActorWorld = GetActorTransform();

	const FTransform GripRelativeToActor =
		GripSocketWorld.GetRelativeTransform(WeaponActorWorld);

	const FTransform NewActorWorld =
		GripRelativeToActor.Inverse() * TargetSocketWorld;

	SetActorTransform(NewActorWorld);

	AttachToComponent(
		CharacterMesh,
		FAttachmentTransformRules::KeepWorldTransform,
		CharacterSocketName
	);
}

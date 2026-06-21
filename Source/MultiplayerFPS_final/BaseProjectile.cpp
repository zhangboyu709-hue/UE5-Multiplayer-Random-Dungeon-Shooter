#include "BaseProjectile.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"

ABaseProjectile::ABaseProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	SetRootComponent(CollisionComp);

	CollisionComp->InitSphereRadius(6.0f);
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionComp->SetCollisionObjectType(ECC_WorldDynamic);

	// 实体子弹：默认只和常见实体发生碰撞。
	// 如果以后你建 Projectile 通道，可以在蓝图里把 Collision Preset 改掉。
	CollisionComp->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	CollisionComp->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	CollisionComp->SetNotifyRigidBodyCollision(true);
	CollisionComp->SetGenerateOverlapEvents(true);

	CollisionComp->OnComponentHit.AddDynamic(this, &ABaseProjectile::OnProjectileHit);
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &ABaseProjectile::OnProjectileOverlap);

	VisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VisualMesh"));
	VisualMesh->SetupAttachment(CollisionComp);
	VisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->InitialSpeed = InitialSpeed;
	ProjectileMovement->MaxSpeed = InitialSpeed;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f;
	ProjectileMovement->bSweepCollision = true;

	DamageTypeClass = UDamageType::StaticClass();
}

void ABaseProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		SetLifeSpan(LifeSeconds);
	}
}

void ABaseProjectile::InitProjectile(
	AController* InInstigatorController,
	AActor* InDamageCauser,
	float InDamage,
	float InInitialSpeed,
	float InLifeSeconds
)
{
	DamageInstigatorController = InInstigatorController;
	DamageCauserActor = InDamageCauser;

	Damage = InDamage;
	InitialSpeed = InInitialSpeed;
	LifeSeconds = InLifeSeconds;

	if (ProjectileMovement)
	{
		ProjectileMovement->InitialSpeed = InitialSpeed;
		ProjectileMovement->MaxSpeed = InitialSpeed;
	}

	if (HasAuthority())
	{
		SetLifeSpan(LifeSeconds);
	}
}

void ABaseProjectile::LaunchInDirection(const FVector& Direction)
{
	const FVector SafeDirection = Direction.GetSafeNormal();

	if (ProjectileMovement)
	{
		ProjectileMovement->Velocity = SafeDirection * InitialSpeed;
	}

	BP_OnProjectileLaunched();
}

void ABaseProjectile::OnProjectileHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit
)
{
	ProcessImpact(OtherActor, Hit);
}

void ABaseProjectile::OnProjectileOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult
)
{
	ProcessImpact(OtherActor, SweepResult);
}

void ABaseProjectile::ProcessImpact(AActor* OtherActor, const FHitResult& Hit)
{
	if (!HasAuthority())
	{
		return;
	}

	if (bHasImpacted)
	{
		return;
	}

	if (!OtherActor || OtherActor == this)
	{
		return;
	}

	// 不打自己、不打拥有者、不打发射者、不打生成自己的武器。
	if (OtherActor == GetOwner() || OtherActor == GetInstigator() || OtherActor == DamageCauserActor)
	{
		return;
	}

	bHasImpacted = true;

	FVector ImpactLocation = GetActorLocation();
	if (!Hit.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = FVector(Hit.ImpactPoint);
	}

	FVector ImpactNormal = -GetActorForwardVector();
	if (!Hit.ImpactNormal.IsNearlyZero())
	{
		ImpactNormal = FVector(Hit.ImpactNormal);
	}

	if (bUseDefaultDamage)
	{
		ApplyDefaultDamage(OtherActor, Hit);
	}

	// 额外玩法入口：燃烧、分裂、生成火焰地面、特殊 Debuff 等。
	BP_OnServerImpact(OtherActor, ImpactLocation, ImpactNormal);

	MulticastPlayImpactFX(ImpactLocation, ImpactNormal);

	if (bDestroyOnImpact)
	{
		Destroy();
	}
}

void ABaseProjectile::ApplyDefaultDamage(AActor* OtherActor, const FHitResult& Hit)
{
	if (!OtherActor)
	{
		return;
	}

	AActor* DamageCauser = DamageCauserActor ? DamageCauserActor : this;
	TSubclassOf<UDamageType> ActualDamageType = DamageTypeClass;
	if (!ActualDamageType)
	{
		ActualDamageType = UDamageType::StaticClass();
	}

	if (DamageRadius > 0.0f)
	{
		TArray<AActor*> IgnoredActors;
		IgnoredActors.Add(this);

		if (GetOwner())
		{
			IgnoredActors.Add(GetOwner());
		}

		if (GetInstigator())
		{
			IgnoredActors.Add(GetInstigator());
		}

		UGameplayStatics::ApplyRadialDamage(
			this,
			Damage,
			GetActorLocation(),
			DamageRadius,
			ActualDamageType,
			IgnoredActors,
			DamageCauser,
			DamageInstigatorController,
			true
		);

		return;
	}

	UGameplayStatics::ApplyDamage(
		OtherActor,
		Damage,
		DamageInstigatorController,
		DamageCauser,
		ActualDamageType
	);
}

void ABaseProjectile::MulticastPlayImpactFX_Implementation(
	FVector ImpactLocation,
	FVector ImpactNormal
)
{
	BP_OnImpactFX(ImpactLocation, ImpactNormal);

	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, ImpactLocation);
	}

	if (ImpactFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ImpactFX,
			ImpactLocation,
			ImpactNormal.Rotation()
		);
	}
}

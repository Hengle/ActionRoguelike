// Fill out your copyright notice in the Description page of Project Settings.


#include "RogueProjectile.h"
#include "Components/SphereComponent.h"
#include "Projectiles/RogueProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "Performance/RogueActorPoolingSubsystem.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RogueProjectile)

// NOTE: With SparseDataClass feature in use, some properties are replaced with "GetXXX()" which is generated automatically by UHT.
// Example: DamageAmount becomes GetDamageAmount() without this function visible in our own header.

TRACE_DECLARE_INT_COUNTER(COUNTER_GAME_ActiveProjectiles, TEXT("Game/ActiveProjectiles"));

ARogueProjectile::ARogueProjectile()
{
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	SphereComp->SetCollisionProfileName("Projectile");
	// Dont bother telling the nav system whenever we move
	SphereComp->SetCanEverAffectNavigation(false);
	RootComponent = SphereComp;

	NiagaraLoopComp = CreateDefaultSubobject<UNiagaraComponent>(TEXT("EffectComp"));
	//NiagaraLoopComp->PoolingMethod = ENCPoolMethod::AutoRelease;
	NiagaraLoopComp->SetupAttachment(RootComponent);

	AudioComp = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComp"));
	AudioComp->SetupAttachment(RootComponent);

	// Custom Projectile Component (for tick management & better homing)
	MoveComp = CreateDefaultSubobject<URogueProjectileMovementComponent>(TEXT("ProjectileMoveComp"));
	MoveComp->bRotationFollowsVelocity = true;
	MoveComp->bInitialVelocityInLocalSpace = true;
	MoveComp->ProjectileGravityScale = 0.0f;
	MoveComp->InitialSpeed = 8000;

	// Directly set bool instead of going through SetReplicates(true) within constructor,
	// Only use SetReplicates() outside constructor
	bReplicates = true;
}


void ARogueProjectile::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	
	// More consistent to bind here compared to Constructor which may fail to bind if Blueprint was created before adding this binding (or when using hotreload)
	// PostInitializeComponent is the preferred way of binding any events.
	SphereComp->OnComponentHit.AddDynamic(this, &ARogueProjectile::OnActorHit);
}


void ARogueProjectile::BeginPlay()
{
	Super::BeginPlay();

	// Can use to fine-tune the pre allocated actor pool by checking how many projectiles are alive during gameplay
	TRACE_COUNTER_INCREMENT(COUNTER_GAME_ActiveProjectiles);
}

void ARogueProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	TRACE_COUNTER_DECREMENT(COUNTER_GAME_ActiveProjectiles);
}


void ARogueProjectile::PoolBeginPlay_Implementation()
{
	MoveComp->Reset();
	
	//NiagaraLoopComp->Activate();
	//AudioComp->Play();
	
	// Unpausing is significantly faster than re-creating renderstates due to Deactivate()
	// Does keep its state around which is OK for our loopable VFX that will mostly be active/in-use
	NiagaraLoopComp->SetPaused(false);
	// Reset to fix ribbon positions
	//NiagaraLoopComp->ResetSystem();
	AudioComp->SetPaused(false);

}


void ARogueProjectile::PoolEndPlay_Implementation()
{
	//NiagaraLoopComp->Deactivate();
	//AudioComp->Stop();
	NiagaraLoopComp->SetPaused(true);
	AudioComp->SetPaused(true);
}


void ARogueProjectile::LifeSpanExpired()
{
	// Skip destroy and instead release to pool
	URogueActorPoolingSubsystem::ReleaseToPool(this);
}


void ARogueProjectile::OnActorHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	Explode();
}


// _Implementation from it being marked as BlueprintNativeEvent
void ARogueProjectile::Explode_Implementation()
{
	// Auto-managed particle pooling
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactVFX, GetActorLocation(), GetActorRotation(), FVector(1), true, true, ENCPoolMethod::AutoRelease);

	UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation());

	UGameplayStatics::PlayWorldCameraShake(this, ImpactShake, GetActorLocation(), GetImpactShakeInnerRadius(), GetImpactShakeOuterRadius());

	//Destroy();
	// Release back to pool instead of destroying
	URogueActorPoolingSubsystem* PoolingSubsystem = GetWorld()->GetSubsystem<URogueActorPoolingSubsystem>();
	PoolingSubsystem->ReleaseToPool(this);
}


#if WITH_EDITOR
// Only required to convert existing properties already stored in Blueprints into the 'new' system
void ARogueProjectile::MoveDataToSparseClassDataStruct() const
{
	// make sure we don't overwrite the sparse data if it has been saved already
	const UBlueprintGeneratedClass* BPClass = Cast<UBlueprintGeneratedClass>(GetClass());
	if (BPClass == nullptr || BPClass->bIsSparseClassDataSerializable == true)
	{
		return;
	}
	
	Super::MoveDataToSparseClassDataStruct();

#if WITH_EDITORONLY_DATA
	// Unreal Header Tool (UHT) will create GetMySparseClassData automatically.
	FProjectileSparseData* SparseClassData = GetProjectileSparseData();

	// Modify these lines to include all Sparse Class Data properties.
	SparseClassData->ImpactShakeInnerRadius = ImpactShakeInnerRadius_DEPRECATED;
	SparseClassData->ImpactShakeOuterRadius = ImpactShakeOuterRadius_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
}
#endif


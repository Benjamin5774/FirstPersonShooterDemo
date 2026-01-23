#include "ProjectileHitEffectComponent.h"
#include "DamageEffectInterface.h"
#include "HealthComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

UProjectileHitEffectComponent::UProjectileHitEffectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	DamageAmount = 25.0f;
	DamageColor = FLinearColor::Red;
	bOnlyOnOtherPlayers = true;
	bTriggerOnce = true;
	bHasTriggered = false;
}

void UProjectileHitEffectComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->OnActorHit.AddDynamic(this, &UProjectileHitEffectComponent::HandleOwnerHit);
	}
}

void UProjectileHitEffectComponent::HandleOwnerHit(AActor* SelfActor, AActor* OtherActor,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bTriggerOnce && bHasTriggered)
	{
		return;
	}

	if (!SelfActor || !OtherActor || SelfActor == OtherActor)
	{
		return;
	}

	if (!SelfActor->HasAuthority())
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(SelfActor->GetInstigator());
	if (!OwnerPawn)
	{
		OwnerPawn = Cast<APawn>(SelfActor->GetOwner());
	}

	APawn* OtherPawn = Cast<APawn>(OtherActor);
	if (!IsValidHitTarget(OwnerPawn, OtherPawn))
	{
		return;
	}

	if (UHealthComponent* HealthComp = OtherActor->FindComponentByClass<UHealthComponent>())
	{
		AController* OwnerController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
		UE_LOG(LogTemp, Log, TEXT("子弹命中触发扣血"));
		const float AppliedDamage = FMath::Abs(DamageAmount);
		HealthComp->ApplyDamage(AppliedDamage, OwnerController);

		SpawnDamageEffect(OwnerController, OtherActor, AppliedDamage);
		PlayHitSoundForOwner(OwnerController);
		bHasTriggered = true;
	}
}

bool UProjectileHitEffectComponent::IsValidHitTarget(APawn* OwnerPawn, APawn* OtherPawn) const
{
	if (!OwnerPawn || !OtherPawn || OwnerPawn == OtherPawn)
	{
		return false;
	}

	if (bOnlyOnOtherPlayers)
	{
		if (!OwnerPawn->IsPlayerControlled() || !OtherPawn->IsPlayerControlled())
		{
			return false;
		}
	}

	return true;
}

void UProjectileHitEffectComponent::SpawnDamageEffect(AController* OwnerController, AActor* HitActor, float AppliedDamage)
{
	if (!DamageEffectClass || !GetWorld() || !HitActor)
	{
		return;
	}

	const FVector Location = HitActor->GetActorLocation();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerController;
	SpawnParams.Instigator = Cast<APawn>(OwnerController ? OwnerController->GetPawn() : nullptr);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* EffectActor = GetWorld()->SpawnActor<AActor>(DamageEffectClass, Location, FRotator::ZeroRotator, SpawnParams);
	if (!EffectActor)
	{
		return;
	}

	EffectActor->SetOwner(OwnerController);

	if (EffectActor->GetClass()->ImplementsInterface(UDamageEffectInterface::StaticClass()))
	{
		IDamageEffectInterface::Execute_InitDamageEffect(EffectActor, -AppliedDamage, DamageColor);
	}
}

void UProjectileHitEffectComponent::PlayHitSoundForOwner(AController* OwnerController)
{
	if (!HitSound)
	{
		return;
	}

	if (APlayerController* PC = Cast<APlayerController>(OwnerController))
	{
		PC->ClientPlaySound(HitSound);
	}
}


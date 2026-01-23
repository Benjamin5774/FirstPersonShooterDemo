#include "ProjectileHitEffectComponent.h"
#include "FPSPlayerState.h"
#include "DamageEffectInterface.h"
#include "GameFramework/Character.h"
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

		if (OwnerController && OwnerController->IsLocalController())
		{
			SpawnDamageEffectLocal(OwnerController, OtherActor, -AppliedDamage);
		}
		else if (AFPSPlayerState* OwnerPS = OwnerPawn ? Cast<AFPSPlayerState>(OwnerPawn->GetPlayerState()) : nullptr)
		{
			OwnerPS->ClientShowDamageNumber(OtherActor, -AppliedDamage, DamageColor, DamageEffectClass);
		}

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

void UProjectileHitEffectComponent::SpawnDamageEffectLocal(AController* OwnerController, AActor* HitActor, float Damage)
{
	if (!DamageEffectClass || !GetWorld() || !HitActor || !OwnerController)
	{
		return;
	}

	FVector SpawnLocation = HitActor->GetActorLocation();
	if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
	{
		if (USkeletalMeshComponent* Mesh = HitCharacter->GetMesh())
		{
			SpawnLocation = Mesh->GetComponentLocation();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerController;
	SpawnParams.Instigator = Cast<APawn>(OwnerController->GetPawn());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* EffectActor = GetWorld()->SpawnActor<AActor>(DamageEffectClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!EffectActor)
	{
		return;
	}

	if (EffectActor->GetClass()->ImplementsInterface(UDamageEffectInterface::StaticClass()))
	{
		IDamageEffectInterface::Execute_InitDamageEffect(EffectActor, Damage, DamageColor);
	}
}


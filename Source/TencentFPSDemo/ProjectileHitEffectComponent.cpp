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
	bool bIsValidTarget = IsValidHitTarget(OwnerPawn, OtherPawn);

	if (bIsValidTarget)
	{
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

	// 无论是否命中有效目标，碰撞后都应该销毁子弹，防止子弹在场景中弹跳
	// 使用延迟销毁，让蓝图事件处理完成后再销毁，避免访问已销毁的组件
	if (SelfActor && SelfActor->HasAuthority())
	{
		// 延迟一帧销毁，确保蓝图事件处理完成
		SelfActor->SetLifeSpan(0.01f);
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


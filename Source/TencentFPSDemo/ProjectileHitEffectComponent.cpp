#include "ProjectileHitEffectComponent.h"
#include "FPSPlayerState.h"
#include "DamageEffectInterface.h"
#include "WeaponBase.h"
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
	HeadshotDamageMultiplier = 2.0f;
	HeadMeshName = TEXT("Head");
	HeadshotDamageColor = FLinearColor::Yellow;
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
			
			// 检测是否爆头
			bool bIsHeadshot = false;
			if (Hit.Component.IsValid() && !HeadMeshName.IsNone())
			{
				FName HitComponentName = Hit.Component->GetFName();
				bIsHeadshot = (HitComponentName == HeadMeshName);
			}

			float AppliedDamage = FMath::Abs(DamageAmount);
			FLinearColor FinalDamageColor = DamageColor;
			if (bIsHeadshot)
			{
				AppliedDamage *= HeadshotDamageMultiplier;
				FinalDamageColor = HeadshotDamageColor;
				UE_LOG(LogTemp, Log, TEXT("爆头！伤害: %.2f (基础伤害 %.2f x %.2f)"), AppliedDamage, DamageAmount, HeadshotDamageMultiplier);
			}
			else
			{
				UE_LOG(LogTemp, Log, TEXT("子弹命中触发扣血"));
			}

			HealthComp->ApplyDamage(AppliedDamage, OwnerController);

			if (OwnerController && OwnerController->IsLocalController())
			{
				SpawnDamageEffectLocal(OwnerController, OtherActor, -AppliedDamage);
			}
			else if (AFPSPlayerState* OwnerPS = OwnerPawn ? Cast<AFPSPlayerState>(OwnerPawn->GetPlayerState()) : nullptr)
			{
				OwnerPS->ClientShowDamageNumber(OtherActor, -AppliedDamage, FinalDamageColor, DamageEffectClass);
			}

			PlayHitSoundForOwner(OwnerController);

			if (AWeaponBase* ShooterWeapon = AWeaponBase::GetWeaponFromPawn(OwnerPawn))
			{
				ShooterWeapon->ClientSetCrosshairState(bIsHeadshot ? ECrosshairState::Headshot : ECrosshairState::Hit);
			}

			bHasTriggered = true;
		}
	}


	if (SelfActor && SelfActor->HasAuthority())
	{

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

	FLinearColor FinalColor = (FMath::Abs(Damage) > DamageAmount * 1.5f) ? HeadshotDamageColor : DamageColor;
	
	if (EffectActor->GetClass()->ImplementsInterface(UDamageEffectInterface::StaticClass()))
	{
		IDamageEffectInterface::Execute_InitDamageEffect(EffectActor, Damage, FinalColor);
	}
}


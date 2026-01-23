#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProjectileHitEffectComponent.generated.h"

class USoundBase;

UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class TENCENTFPSDEMO_API UProjectileHitEffectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UProjectileHitEffectComponent();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	float DamageAmount;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	FLinearColor DamageColor;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	TSubclassOf<AActor> DamageEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Damage|Audio")
	USoundBase* HitSound;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	bool bOnlyOnOtherPlayers;

	UPROPERTY(EditDefaultsOnly, Category = "Damage")
	bool bTriggerOnce;

	bool bHasTriggered;

	UFUNCTION()
	void HandleOwnerHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit);

	bool IsValidHitTarget(APawn* OwnerPawn, APawn* OtherPawn) const;
	void SpawnDamageEffect(AController* OwnerController, const FVector& Location, float AppliedDamage);
	void PlayHitSoundForOwner(AController* OwnerController);
};


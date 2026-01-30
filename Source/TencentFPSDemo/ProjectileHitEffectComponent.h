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

	/** 爆头伤害倍数（例如 2.0 表示双倍伤害） */
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Headshot", meta = (ClampMin = "1.0", UIMin = "1.0"))
	float HeadshotDamageMultiplier;

	/** 头部网格的名称（例如 "Head"），用于检测爆头 */
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Headshot")
	FName HeadMeshName;

	/** 爆头时的伤害数字颜色（可与普通伤害区分） */
	UPROPERTY(EditDefaultsOnly, Category = "Damage|Headshot")
	FLinearColor HeadshotDamageColor;

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
	void PlayHitSoundForOwner(AController* OwnerController);
	void SpawnDamageEffectLocal(AController* OwnerController, AActor* HitActor, float Damage);
};


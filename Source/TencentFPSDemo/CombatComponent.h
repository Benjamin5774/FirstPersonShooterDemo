#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

class AActor;
class USoundBase;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TENCENTFPSDEMO_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	void StartFire();

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);

	UFUNCTION(Client, Reliable)
	void ClientShowHitFeedback(FVector_NetQuantize Location, float DamageAmount);

protected:
	virtual void BeginPlay() override;

	void HandleLineTrace(const FVector& Origin, const FVector& Dir);

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float TraceDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float Damage;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitEffect")
	TSubclassOf<AActor> HitEffectClass;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitEffect")
	FLinearColor HitTextColor;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitEffect")
	float HitEffectZOffset;

	UPROPERTY(EditDefaultsOnly, Category = "Combat|HitEffect")
	USoundBase* HitSound;
};


#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TENCENTFPSDEMO_API UCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatComponent();

	void StartFire();

	UFUNCTION(Server, Reliable)
	void ServerFire(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir);

protected:
	virtual void BeginPlay() override;

	void HandleLineTrace(const FVector& Origin, const FVector& Dir);

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float TraceDistance;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float Damage;
};


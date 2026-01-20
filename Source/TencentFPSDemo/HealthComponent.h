#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AController;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TENCENTFPSDEMO_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	void ApplyDamage(float Amount, AController* InstigatorController);
	float GetHealth() const { return Health; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = "Health")
	float Health;

	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float MaxHealth;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	void HandleDeath(AController* InstigatorController);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};


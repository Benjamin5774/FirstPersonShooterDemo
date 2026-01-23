#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"

class AController;
class APlayerController;
class UUserWidget;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, float, NewHealth, float, MaxHealth);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TENCENTFPSDEMO_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHealthComponent();

	UFUNCTION(BlueprintCallable, Category = "Health")
	void ApplyDamage(float Amount, AController* InstigatorController);

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetHealth() const { return Health; }

	UFUNCTION(BlueprintCallable, Category = "Health")
	float GetMaxHealth() const { return MaxHealth; }

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FOnHealthChanged OnHealthChanged;
	

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(ReplicatedUsing = OnRep_Health, EditDefaultsOnly, Category = "Health")
	float Health;

	UPROPERTY(EditDefaultsOnly, Category = "Health")
	float MaxHealth;

	UPROPERTY(EditDefaultsOnly, Category = "HitEffect")
	TSubclassOf<UUserWidget> HitEffectWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "HitEffect")
	float HitEffectDuration;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	void HandleDeath(AController* InstigatorController);

	UFUNCTION(Client, Reliable)
	void ClientShowHitEffect(float Duration);

	void RemoveHitEffect();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Transient)
	UUserWidget* ActiveHitWidget;

	FTimerHandle HitEffectTimerHandle;
};


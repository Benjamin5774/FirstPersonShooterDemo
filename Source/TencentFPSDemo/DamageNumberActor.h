#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DamageEffectInterface.h"
#include "DamageNumberActor.generated.h"

class UWidgetComponent;
class UUserWidget;

UCLASS()
class TENCENTFPSDEMO_API ADamageNumberActor : public AActor, public IDamageEffectInterface
{
	GENERATED_BODY()

public:
	ADamageNumberActor();

	virtual void Tick(float DeltaSeconds) override;

	virtual void InitDamageEffect_Implementation(float Damage, FLinearColor Color) override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "DamageEffect")
	USceneComponent* Root;

	UPROPERTY(VisibleAnywhere, Category = "DamageEffect")
	UWidgetComponent* WidgetComponent;

	UPROPERTY(EditDefaultsOnly, Category = "DamageEffect")
	TSubclassOf<UUserWidget> DamageWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "DamageEffect")
	float RiseSpeed;

	UPROPERTY(EditDefaultsOnly, Category = "DamageEffect")
	float LifeTime;
};


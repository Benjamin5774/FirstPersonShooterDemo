#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MovementSoundComponent.generated.h"

class UAudioComponent;
class USoundBase;

UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class TENCENTFPSDEMO_API UMovementSoundComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMovementSoundComponent();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool ShouldPlayForOwner() const;
	float GetOwnerSpeed2D() const;
	void UpdateMovementAudio(float Speed2D);

	UPROPERTY(EditAnywhere, Category = "Audio")
	USoundBase* MoveSound;

	UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0"))
	float VolumeMultiplier;

	UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0"))
	float MinSpeedToPlay;

	UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.0"))
	float MaxSpeed;

	UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.1"))
	float MinPitch;

	UPROPERTY(EditAnywhere, Category = "Audio", meta = (ClampMin = "0.1"))
	float MaxPitch;

	UPROPERTY()
	UAudioComponent* MoveAudioComponent;
};


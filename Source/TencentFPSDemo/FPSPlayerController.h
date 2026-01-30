#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "FPSPlayerController.generated.h"

class AWeaponBase;

UCLASS()
class TENCENTFPSDEMO_API AFPSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AFPSPlayerController();

	virtual void SetupInputComponent() override;

	UFUNCTION(Server, Reliable)
	void ServerSetReadyForStart();

	UFUNCTION(Server, Reliable)
	void ServerSetReadyForRestart();

	UFUNCTION(Client, Reliable)
	void ClientPlaySound2D(USoundBase* Sound, float Volume);

	UFUNCTION(Client, Reliable)
	void ClientShowKillIcon(float Duration);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Reload")
	FKey ReloadKey = EKeys::E;

	// 开镜按键
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|ADS")
	FKey ADSKey = EKeys::RightMouseButton;

protected:

	void OnReloadPressed();
	void OnADSPressed();
	void OnADSReleased();
	AWeaponBase* GetCurrentWeapon() const;
};


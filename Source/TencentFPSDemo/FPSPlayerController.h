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

	// 换弹按键（可在编辑器中调整）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Reload")
	FKey ReloadKey = EKeys::E;

	// 开镜按键（按住开镜，松开恢复）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Aim")
	FKey AimKey = EKeys::RightMouseButton;

protected:
	// 处理换弹输入
	void OnReloadPressed();

	void OnAimPressed();
	void OnAimReleased();

	// 获取当前武器
	AWeaponBase* GetCurrentWeapon() const;
};


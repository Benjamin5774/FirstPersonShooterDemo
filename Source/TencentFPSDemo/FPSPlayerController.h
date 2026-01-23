#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "FPSPlayerController.generated.h"

UCLASS()
class TENCENTFPSDEMO_API AFPSPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	UFUNCTION(Server, Reliable)
	void ServerSetReadyForStart();

	UFUNCTION(Server, Reliable)
	void ServerSetReadyForRestart();
};


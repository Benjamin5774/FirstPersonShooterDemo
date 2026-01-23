#include "FPSPlayerController.h"
#include "FPSGameModeBase.h"

void AFPSPlayerController::ServerSetReadyForStart_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AFPSGameModeBase* GameMode = World->GetAuthGameMode<AFPSGameModeBase>())
		{
			GameMode->HandlePlayerReady(this, false);
		}
	}
}

void AFPSPlayerController::ServerSetReadyForRestart_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AFPSGameModeBase* GameMode = World->GetAuthGameMode<AFPSGameModeBase>())
		{
			GameMode->HandlePlayerReady(this, true);
		}
	}
}


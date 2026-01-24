#include "FPSPlayerController.h"
#include "FPSGameModeBase.h"
#include "FPSHUD.h"
#include "Kismet/GameplayStatics.h"

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

void AFPSPlayerController::ClientPlaySound2D_Implementation(USoundBase* Sound, float Volume)
{
	if (!Sound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, Sound, Volume);
}

void AFPSPlayerController::ClientShowKillIcon_Implementation(float Duration)
{
	if (AFPSHUD* HUD = Cast<AFPSHUD>(GetHUD()))
	{
		HUD->ShowKillIcon(Duration);
	}
}


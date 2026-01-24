#include "FPSHUD.h"
#include "FPSGameModeBase.h"
#include "FPSGameState.h"
#include "FPSPlayerController.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"

void AFPSHUD::BeginPlay()
{
	Super::BeginPlay();
	TryInitWidgets();
}

void AFPSHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ScoreboardWidget || !GameOverWidget || !StartWidget || !ReplayWidget)
	{
		TryInitWidgets();
	}

	UpdateScoreboard();
	ShowGameOverIfNeeded();
	UpdateStartWidget();
	UpdateReplayWidget();
	UpdateInputMode();
}

void AFPSHUD::TryInitWidgets()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	AFPSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AFPSGameState>() : nullptr;
	const UClass* GameModeClass = GameState ? GameState->GameModeClass : nullptr;
	const AFPSGameModeBase* GameModeCDO = GameModeClass
		? Cast<AFPSGameModeBase>(GameModeClass->GetDefaultObject())
		: nullptr;
	if (!GameModeCDO)
	{
		return;
	}

	if (!ScoreboardWidget && GameModeCDO->GetScoreboardWidgetClass())
	{
		ScoreboardWidget = CreateWidget<UUserWidget>(PC, GameModeCDO->GetScoreboardWidgetClass());
		if (ScoreboardWidget)
		{
			ScoreboardWidget->AddToViewport();
			TimerText = Cast<UTextBlock>(ScoreboardWidget->GetWidgetFromName(TEXT("TimerText")));
			TeamAKillText = Cast<UTextBlock>(ScoreboardWidget->GetWidgetFromName(TEXT("TeamAKillText")));
			TeamBKillText = Cast<UTextBlock>(ScoreboardWidget->GetWidgetFromName(TEXT("TeamBKillText")));
		}
	}

	if (!GameOverWidget && GameModeCDO->GetGameOverWidgetClass())
	{
		GameOverWidget = CreateWidget<UUserWidget>(PC, GameModeCDO->GetGameOverWidgetClass());
		if (GameOverWidget)
		{
			GameOverDisplayText = Cast<UTextBlock>(GameOverWidget->GetWidgetFromName(TEXT("GameOverDisplayText")));
		}
	}

	if (!StartWidget && GameModeCDO->GetStartWidgetClass())
	{
		StartWidget = CreateWidget<UUserWidget>(PC, GameModeCDO->GetStartWidgetClass());
		if (StartWidget)
		{
			StartReadyText = Cast<UTextBlock>(StartWidget->GetWidgetFromName(TEXT("CurrentReadPlayer")));
			StartButton = Cast<UButton>(StartWidget->GetWidgetFromName(TEXT("GameStart")));
			if (StartButton)
			{
				StartButton->OnClicked.AddDynamic(this, &AFPSHUD::HandleStartButtonClicked);
			}
		}
	}

	if (!ReplayWidget && GameModeCDO->GetReplayWidgetClass())
	{
		ReplayWidget = CreateWidget<UUserWidget>(PC, GameModeCDO->GetReplayWidgetClass());
		if (ReplayWidget)
		{
			ReplayReadyText = Cast<UTextBlock>(ReplayWidget->GetWidgetFromName(TEXT("CurrentReadPlayer")));
			RestartButton = Cast<UButton>(ReplayWidget->GetWidgetFromName(TEXT("GameRestart")));
			if (RestartButton)
			{
				RestartButton->OnClicked.AddDynamic(this, &AFPSHUD::HandleRestartButtonClicked);
			}
		}
	}

	if (!KillIconWidget && GameModeCDO->GetKillIconWidgetClass())
	{
		KillIconWidget = CreateWidget<UUserWidget>(PC, GameModeCDO->GetKillIconWidgetClass());
	}
}

void AFPSHUD::UpdateScoreboard()
{
	if (!ScoreboardWidget)
	{
		return;
	}

	AFPSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AFPSGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	if (TimerText)
	{
		TimerText->SetText(FText::AsNumber(GameState->RemainingTime));
	}

	if (TeamAKillText)
	{
		TeamAKillText->SetText(FText::AsNumber(GameState->TeamAScore));
	}

	if (TeamBKillText)
	{
		TeamBKillText->SetText(FText::AsNumber(GameState->TeamBScore));
	}
}

void AFPSHUD::ShowGameOverIfNeeded()
{
	if (bGameOverWidgetShown || !GameOverWidget)
	{
		return;
	}

	AFPSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AFPSGameState>() : nullptr;
	if (!GameState || !GameState->bMatchOver || GameState->bWaitingForRestart)
	{
		return;
	}

	if (GameOverDisplayText)
	{
		FString ResultText = TEXT("Draw");
		if (GameState->WinningTeamId == 0)
		{
			ResultText = TEXT("Team A Wins");
		}
		else if (GameState->WinningTeamId == 1)
		{
			ResultText = TEXT("Team B Wins");
		}
		GameOverDisplayText->SetText(FText::FromString(ResultText));
	}

	GameOverWidget->AddToViewport();
	bGameOverWidgetShown = true;
}

void AFPSHUD::UpdateStartWidget()
{
	if (!StartWidget)
	{
		return;
	}

	AFPSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AFPSGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	const bool bShouldShow = GameState->bWaitingForStart;
	if (bShouldShow && !bStartWidgetShown)
	{
		if (!StartWidget->IsInViewport())
		{
			StartWidget->AddToViewport();
		}
		bStartWidgetShown = true;
	}
	else if (!bShouldShow && bStartWidgetShown)
	{
		StartWidget->RemoveFromParent();
		bStartWidgetShown = false;
	}

	if (bShouldShow)
	{
		UpdateReadyText(StartReadyText, GameState->ReadyPlayerCount, GameState->PlayerArray.Num());
	}
}

void AFPSHUD::UpdateReplayWidget()
{
	if (!ReplayWidget)
	{
		return;
	}

	AFPSGameState* GameState = GetWorld() ? GetWorld()->GetGameState<AFPSGameState>() : nullptr;
	if (!GameState)
	{
		return;
	}

	const bool bShouldShow = GameState->bWaitingForRestart;
	if (bShouldShow && !bReplayWidgetShown)
	{
		if (!ReplayWidget->IsInViewport())
		{
			ReplayWidget->AddToViewport();
		}
		bReplayWidgetShown = true;
	}
	else if (!bShouldShow && bReplayWidgetShown)
	{
		ReplayWidget->RemoveFromParent();
		bReplayWidgetShown = false;
	}

	if (bShouldShow)
	{
		UpdateReadyText(ReplayReadyText, GameState->ReadyPlayerCount, GameState->PlayerArray.Num());
	}

	if (bGameOverWidgetShown && (bShouldShow || !GameState->bMatchOver) && GameOverWidget)
	{
		GameOverWidget->RemoveFromParent();
		bGameOverWidgetShown = false;
	}
}

void AFPSHUD::UpdateReadyText(UTextBlock* ReadyText, int32 ReadyCount, int32 TotalPlayers) const
{
	if (!ReadyText)
	{
		return;
	}

	const FString ReadyString = FString::Printf(TEXT("%d/%d"), ReadyCount, TotalPlayers);
	ReadyText->SetText(FText::FromString(ReadyString));
}

void AFPSHUD::UpdateInputMode()
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC)
	{
		return;
	}

	const bool bShouldEnableUI = bStartWidgetShown || bReplayWidgetShown;
	if (bShouldEnableUI == bUIInputEnabled)
	{
		return;
	}

	if (bShouldEnableUI)
	{
		FInputModeUIOnly InputMode;
		if (bStartWidgetShown && StartWidget)
		{
			InputMode.SetWidgetToFocus(StartWidget->TakeWidget());
		}
		else if (bReplayWidgetShown && ReplayWidget)
		{
			InputMode.SetWidgetToFocus(ReplayWidget->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		FInputModeGameOnly InputMode;
		PC->SetInputMode(InputMode);
		PC->SetShowMouseCursor(false);
	}

	bUIInputEnabled = bShouldEnableUI;
}

void AFPSHUD::HandleStartButtonClicked()
{
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetOwningPlayerController()))
	{
		PC->ServerSetReadyForStart();
	}
}

void AFPSHUD::HandleRestartButtonClicked()
{
	if (AFPSPlayerController* PC = Cast<AFPSPlayerController>(GetOwningPlayerController()))
	{
		PC->ServerSetReadyForRestart();
	}
}

void AFPSHUD::ShowKillIcon(float Duration)
{
	if (!KillIconWidget)
	{
		TryInitWidgets();
	}

	if (!KillIconWidget)
	{
		return;
	}

	if (!KillIconWidget->IsInViewport())
	{
		KillIconWidget->AddToViewport();
	}

	bKillIconVisible = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(KillIconTimerHandle);
		World->GetTimerManager().SetTimer(
			KillIconTimerHandle, this, &AFPSHUD::HideKillIcon, FMath::Max(0.01f, Duration), false);
	}
}

void AFPSHUD::HideKillIcon()
{
	if (KillIconWidget && bKillIconVisible)
	{
		KillIconWidget->RemoveFromParent();
		bKillIconVisible = false;
	}
}


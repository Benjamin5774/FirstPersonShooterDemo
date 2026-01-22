#include "FPSHUD.h"
#include "FPSGameModeBase.h"
#include "FPSGameState.h"
#include "Blueprint/UserWidget.h"
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

	if (!ScoreboardWidget || !GameOverWidget)
	{
		TryInitWidgets();
	}

	UpdateScoreboard();
	ShowGameOverIfNeeded();
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
	if (!GameState || !GameState->bMatchOver)
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


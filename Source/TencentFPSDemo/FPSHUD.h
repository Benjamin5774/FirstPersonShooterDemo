#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FPSHUD.generated.h"

class UUserWidget;
class UTextBlock;
class UButton;

UCLASS()
class TENCENTFPSDEMO_API AFPSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void ShowKillIcon(float Duration);

private:
	void TryInitWidgets();
	void UpdateScoreboard();
	void ShowGameOverIfNeeded();
	void UpdateStartWidget();
	void UpdateReplayWidget();
	void UpdateReadyText(UTextBlock* ReadyText, int32 ReadyCount, int32 TotalPlayers) const;
	void UpdateInputMode();
	void HideKillIcon();

	UFUNCTION()
	void HandleStartButtonClicked();

	UFUNCTION()
	void HandleRestartButtonClicked();

	UPROPERTY()
	UUserWidget* ScoreboardWidget;

	UPROPERTY()
	UUserWidget* GameOverWidget;

	UPROPERTY()
	UUserWidget* StartWidget;

	UPROPERTY()
	UUserWidget* ReplayWidget;

	UPROPERTY()
	UTextBlock* TimerText;

	UPROPERTY()
	UTextBlock* TeamAKillText;

	UPROPERTY()
	UTextBlock* TeamBKillText;

	UPROPERTY()
	UTextBlock* GameOverDisplayText;

	UPROPERTY()
	UUserWidget* KillIconWidget;

	UPROPERTY()
	UTextBlock* StartReadyText;

	UPROPERTY()
	UTextBlock* ReplayReadyText;

	UPROPERTY()
	UButton* StartButton;

	UPROPERTY()
	UButton* RestartButton;

	bool bGameOverWidgetShown = false;
	bool bStartWidgetShown = false;
	bool bReplayWidgetShown = false;
	bool bUIInputEnabled = false;
	bool bKillIconVisible = false;

	FTimerHandle KillIconTimerHandle;
};


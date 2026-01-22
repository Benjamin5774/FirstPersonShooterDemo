#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "FPSHUD.generated.h"

class UUserWidget;
class UTextBlock;

UCLASS()
class TENCENTFPSDEMO_API AFPSHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	void TryInitWidgets();
	void UpdateScoreboard();
	void ShowGameOverIfNeeded();

	UPROPERTY()
	UUserWidget* ScoreboardWidget;

	UPROPERTY()
	UUserWidget* GameOverWidget;

	UPROPERTY()
	UTextBlock* TimerText;

	UPROPERTY()
	UTextBlock* TeamAKillText;

	UPROPERTY()
	UTextBlock* TeamBKillText;

	UPROPERTY()
	UTextBlock* GameOverDisplayText;

	bool bGameOverWidgetShown = false;
};


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "FPSGameState.generated.h"

UCLASS()
class TENCENTFPSDEMO_API AFPSGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AFPSGameState();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 TeamAScore;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 TeamBScore;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 RemainingTime;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	bool bMatchOver;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 WinningTeamId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bMatchStarted;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bWaitingForStart;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	bool bWaitingForRestart;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Match")
	int32 ReadyPlayerCount;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};


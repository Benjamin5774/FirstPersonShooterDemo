#include "FPSGameState.h"
#include "Net/UnrealNetwork.h"

AFPSGameState::AFPSGameState()
{
	TeamAScore = 0;
	TeamBScore = 0;
	RemainingTime = 0;
	bMatchOver = false;
	WinningTeamId = -1;
	bMatchStarted = false;
	bWaitingForStart = true;
	bWaitingForRestart = false;
	ReadyPlayerCount = 0;
}

//Replicated match state: scores, timer, win condition, ready count.
//复制的比赛状态：比分、计时、胜利条件、就绪人数。
void AFPSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AFPSGameState, TeamAScore);
	DOREPLIFETIME(AFPSGameState, TeamBScore);
	DOREPLIFETIME(AFPSGameState, RemainingTime);
	DOREPLIFETIME(AFPSGameState, bMatchOver);
	DOREPLIFETIME(AFPSGameState, WinningTeamId);
	DOREPLIFETIME(AFPSGameState, bMatchStarted);
	DOREPLIFETIME(AFPSGameState, bWaitingForStart);
	DOREPLIFETIME(AFPSGameState, bWaitingForRestart);
	DOREPLIFETIME(AFPSGameState, ReadyPlayerCount);
}


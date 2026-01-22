#include "FPSGameState.h"
#include "Net/UnrealNetwork.h"

AFPSGameState::AFPSGameState()
{
	TeamAScore = 0;
	TeamBScore = 0;
	RemainingTime = 0;
	bMatchOver = false;
	WinningTeamId = -1;
}

void AFPSGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFPSGameState, TeamAScore);
	DOREPLIFETIME(AFPSGameState, TeamBScore);
	DOREPLIFETIME(AFPSGameState, RemainingTime);
	DOREPLIFETIME(AFPSGameState, bMatchOver);
	DOREPLIFETIME(AFPSGameState, WinningTeamId);
}


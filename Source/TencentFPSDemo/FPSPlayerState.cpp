#include "FPSPlayerState.h"
#include "Net/UnrealNetwork.h"

AFPSPlayerState::AFPSPlayerState()
{
	TeamId = 0;
	Kills = 0;
	Deaths = 0;
}

void AFPSPlayerState::SetTeamId(int32 InTeamId)
{
	if (!HasAuthority())
	{
		return;
	}

	TeamId = InTeamId;
}

void AFPSPlayerState::AddKill()
{
	if (!HasAuthority())
	{
		return;
	}

	++Kills;
}

void AFPSPlayerState::AddDeath()
{
	if (!HasAuthority())
	{
		return;
	}

	++Deaths;
}

void AFPSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFPSPlayerState, TeamId);
	DOREPLIFETIME(AFPSPlayerState, Kills);
	DOREPLIFETIME(AFPSPlayerState, Deaths);
}


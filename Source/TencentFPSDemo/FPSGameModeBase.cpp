#include "FPSGameModeBase.h"
#include "FPSCharacter.h"
#include "FPSPlayerState.h"
#include "GameFramework/GameStateBase.h"

AFPSGameModeBase::AFPSGameModeBase()
{
	DefaultPawnClass = AFPSCharacter::StaticClass();
	PlayerStateClass = AFPSPlayerState::StaticClass();

	MaxPlayers = 4;
	TeamCount = 2;
}

void AFPSGameModeBase::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
	FString& ErrorMessage)
{
	const int32 CurrentPlayers = GameState ? GameState->PlayerArray.Num() : 0;
	if (CurrentPlayers >= MaxPlayers)
	{
		ErrorMessage = TEXT("ServerFull");
		return;
	}

	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

void AFPSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);

	AFPSPlayerState* FPSPlayerState = NewPlayer ? Cast<AFPSPlayerState>(NewPlayer->PlayerState) : nullptr;
	if (!FPSPlayerState || TeamCount <= 0)
	{
		return;
	}

	const int32 PlayerIndex = GameState ? (GameState->PlayerArray.Num() - 1) : 0;
	const int32 TeamId = PlayerIndex % TeamCount;
	FPSPlayerState->SetTeamId(TeamId);
}

void AFPSGameModeBase::OnPlayerKilled(AController* Killer, AController* Victim)
{
	AFPSPlayerState* KillerPS = Killer ? Cast<AFPSPlayerState>(Killer->PlayerState) : nullptr;
	AFPSPlayerState* VictimPS = Victim ? Cast<AFPSPlayerState>(Victim->PlayerState) : nullptr;

	if (VictimPS)
	{
		VictimPS->AddDeath();
	}

	if (KillerPS && KillerPS != VictimPS)
	{
		KillerPS->AddKill();
	}
}


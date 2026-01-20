#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FPSGameModeBase.generated.h"

UCLASS()
class TENCENTFPSDEMO_API AFPSGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFPSGameModeBase();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;

	void OnPlayerKilled(AController* Killer, AController* Victim);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 MaxPlayers;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 TeamCount;
};


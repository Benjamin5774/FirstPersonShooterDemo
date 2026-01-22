#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "FPSGameModeBase.generated.h"
class APawn;
class AWeaponBase;
class APlayerStart;
class UUserWidget;
class AFPSGameState;

UCLASS()
class TENCENTFPSDEMO_API AFPSGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:
	AFPSGameModeBase();

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId,
		FString& ErrorMessage) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	void OnPlayerKilled(AController* Killer, AController* Victim);
	void RequestRespawn(AController* Controller, APawn* DeadPawn);

	TSubclassOf<UUserWidget> GetScoreboardWidgetClass() const { return ScoreboardWidgetClass; }
	TSubclassOf<UUserWidget> GetGameOverWidgetClass() const { return GameOverWidgetClass; }

protected:
	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 MaxPlayers;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	int32 TeamCount;

	UPROPERTY(EditDefaultsOnly, Category = "Match")
	float RespawnDelay;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Respawn")
	bool bKeepWeaponOnRespawn;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Respawn")
	FName WeaponComponentName;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Spawn")
	float SpawnCheckRadius;

	UPROPERTY(EditDefaultsOnly, Category = "Match|Score")
	int32 MatchTimeSeconds;

	UPROPERTY(EditDefaultsOnly, Category = "Match|UI")
	TSubclassOf<UUserWidget> ScoreboardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Match|UI")
	TSubclassOf<UUserWidget> GameOverWidgetClass;

private:
	void RespawnPlayer(AController* Controller);
	AWeaponBase* FindWeaponFromPawn(APawn* Pawn) const;
	AWeaponBase* FindWeaponFromNamedComponent(APawn* Pawn) const;
	bool IsPlayerStartFree(const APlayerStart* Start) const;
	void HandleMatchTimerTick();
	void EndMatchIfNeeded();
	AFPSGameState* GetFPSGameState() const;

	TMap<TWeakObjectPtr<AController>, TWeakObjectPtr<AWeaponBase>> PendingRespawnWeapons;
	FTimerHandle MatchTimerHandle;
};


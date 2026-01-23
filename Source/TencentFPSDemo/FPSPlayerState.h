#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "FPSPlayerState.generated.h"

UCLASS()
class TENCENTFPSDEMO_API AFPSPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AFPSPlayerState();

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Team")
	int32 TeamId;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 Kills;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Score")
	int32 Deaths;

	void SetTeamId(int32 InTeamId);
	void AddKill();
	void AddDeath();

	UFUNCTION(Client, Reliable)
	void ClientShowDamageNumber(AActor* HitActor, float Damage, FLinearColor Color, TSubclassOf<AActor> DamageEffectClass);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};


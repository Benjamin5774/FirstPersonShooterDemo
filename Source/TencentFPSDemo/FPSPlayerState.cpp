#include "FPSPlayerState.h"
#include "DamageEffectInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
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

void AFPSPlayerState::ClientShowDamageNumber_Implementation(AActor* HitActor, float Damage, FLinearColor Color,
	TSubclassOf<AActor> DamageEffectClass)
{
	if (!HitActor || !DamageEffectClass || !GetWorld())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!PC)
	{
		return;
	}

	FVector SpawnLocation = HitActor->GetActorLocation();
	if (ACharacter* HitCharacter = Cast<ACharacter>(HitActor))
	{
		if (USkeletalMeshComponent* Mesh = HitCharacter->GetMesh())
		{
			SpawnLocation = Mesh->GetComponentLocation();
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = PC;
	SpawnParams.Instigator = PC->GetPawn();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* EffectActor = GetWorld()->SpawnActor<AActor>(DamageEffectClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!EffectActor)
	{
		return;
	}

	if (EffectActor->GetClass()->ImplementsInterface(UDamageEffectInterface::StaticClass()))
	{
		IDamageEffectInterface::Execute_InitDamageEffect(EffectActor, Damage, Color);
	}
}

void AFPSPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AFPSPlayerState, TeamId);
	DOREPLIFETIME(AFPSPlayerState, Kills);
	DOREPLIFETIME(AFPSPlayerState, Deaths);
}


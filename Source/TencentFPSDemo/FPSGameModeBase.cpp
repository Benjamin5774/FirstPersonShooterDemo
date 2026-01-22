#include "FPSGameModeBase.h"
#include "FPSPlayerState.h"
#include "EngineUtils.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "WeaponBase.h"
#include "UObject/UnrealType.h"

AFPSGameModeBase::AFPSGameModeBase()
{
	PlayerStateClass = AFPSPlayerState::StaticClass();

	MaxPlayers = 4;
	TeamCount = 2;
	RespawnDelay = 0.1f;
	bKeepWeaponOnRespawn = false;
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

AActor* AFPSGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Player || !GetWorld())
	{
		return Super::ChoosePlayerStart_Implementation(Player);
	}

	int32 PlayerId = 0;
	if (APlayerState* PlayerState = Player->GetPlayerState<APlayerState>())
	{
		PlayerId = PlayerState->GetPlayerId();
	}

	const FString TargetName = (PlayerId == 0) ? TEXT("PlayerStart")
		: FString::Printf(TEXT("PlayerStart%d"), PlayerId);

	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		if (It->GetName() == TargetName)
		{
			return *It;
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
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

void AFPSGameModeBase::RequestRespawn(AController* Controller, APawn* DeadPawn)
{
	if (!Controller)
	{
		return;
	}

	if (DeadPawn)
	{
		if (bKeepWeaponOnRespawn)
		{
			if (AWeaponBase* Weapon = FindWeaponFromPawn(DeadPawn))
			{
				PendingRespawnWeapons.Add(Controller, Weapon);
				Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				Weapon->SetActorHiddenInGame(true);
				Weapon->SetActorEnableCollision(false);
			}
		}
		else
		{
			if (AWeaponBase* Weapon = FindWeaponFromPawn(DeadPawn))
			{
				Weapon->Destroy();
			}
		}

		DeadPawn->DetachFromControllerPendingDestroy();
		DeadPawn->Destroy();
	}

	if (UWorld* World = GetWorld())
	{
		FTimerHandle RespawnTimerHandle;
		FTimerDelegate RespawnDelegate;
		RespawnDelegate.BindUObject(this, &AFPSGameModeBase::RespawnPlayer, Controller);
		World->GetTimerManager().SetTimer(RespawnTimerHandle, RespawnDelegate, RespawnDelay, false);
	}
}

void AFPSGameModeBase::RespawnPlayer(AController* Controller)
{
	if (!Controller)
	{
		return;
	}

	RestartPlayer(Controller);

	if (bKeepWeaponOnRespawn)
	{
		TWeakObjectPtr<AWeaponBase>* FoundWeapon = PendingRespawnWeapons.Find(Controller);
		if (FoundWeapon && FoundWeapon->IsValid())
		{
			APawn* NewPawn = Controller->GetPawn();
			AWeaponBase* Weapon = FoundWeapon->Get();
			if (NewPawn && Weapon)
			{
				Weapon->SetActorHiddenInGame(false);
				Weapon->SetActorEnableCollision(true);
				Weapon->EquipToPawn(NewPawn);
			}
		}

		PendingRespawnWeapons.Remove(Controller);
	}
}

AWeaponBase* AFPSGameModeBase::FindWeaponFromPawn(APawn* Pawn) const
{
	if (!Pawn)
	{
		return nullptr;
	}

	if (!WeaponComponentName.IsNone())
	{
		if (AWeaponBase* Weapon = FindWeaponFromNamedComponent(Pawn))
		{
			return Weapon;
		}
	}

	TArray<AActor*> AttachedActors;
	Pawn->GetAttachedActors(AttachedActors);
	for (AActor* Actor : AttachedActors)
	{
		if (AWeaponBase* Weapon = Cast<AWeaponBase>(Actor))
		{
			return Weapon;
		}
	}

	return nullptr;
}

AWeaponBase* AFPSGameModeBase::FindWeaponFromNamedComponent(APawn* Pawn) const
{
	if (!Pawn || WeaponComponentName.IsNone())
	{
		return nullptr;
	}

	TInlineComponentArray<UActorComponent*> Components;
	Pawn->GetComponents(Components);

	for (UActorComponent* Component : Components)
	{
		if (!Component || Component->GetFName() != WeaponComponentName)
		{
			continue;
		}

		for (TFieldIterator<FObjectProperty> PropIt(Component->GetClass()); PropIt; ++PropIt)
		{
			if (PropIt->PropertyClass && PropIt->PropertyClass->IsChildOf(AWeaponBase::StaticClass()))
			{
				AWeaponBase* Weapon = Cast<AWeaponBase>(PropIt->GetObjectPropertyValue_InContainer(Component));
				if (Weapon)
				{
					return Weapon;
				}
			}
		}
	}

	return nullptr;
}


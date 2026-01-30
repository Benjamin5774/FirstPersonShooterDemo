#include "FPSGameModeBase.h"
#include "FPSGameState.h"
#include "FPSHUD.h"
#include "FPSPlayerController.h"
#include "FPSPlayerState.h"
#include "EngineUtils.h"
#include "Components/ActorComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "WeaponBase.h"
#include "UObject/UnrealType.h"

AFPSGameModeBase::AFPSGameModeBase()
{
	PlayerStateClass = AFPSPlayerState::StaticClass();
	GameStateClass = AFPSGameState::StaticClass();
	HUDClass = AFPSHUD::StaticClass();
	PlayerControllerClass = AFPSPlayerController::StaticClass();

	MaxPlayers = 4;
	TeamCount = 2;
	RespawnDelay = 0.1f;
	bKeepWeaponOnRespawn = false;
	SpawnCheckRadius = 100.0f;
	MatchTimeSeconds = 300;
	KillSoundVolume = 1.0f;
	KillIconDuration = 1.0f;
}

void AFPSGameModeBase::BeginPlay()
{
	Super::BeginPlay();

	if (AFPSGameState* FPSGameState = GetFPSGameState())
	{
		FPSGameState->RemainingTime = MatchTimeSeconds;
		FPSGameState->TeamAScore = 0;
		FPSGameState->TeamBScore = 0;
		FPSGameState->bMatchOver = false;
		FPSGameState->WinningTeamId = -1;
		FPSGameState->bMatchStarted = false;
		FPSGameState->bWaitingForStart = true;
		FPSGameState->bWaitingForRestart = false;
		FPSGameState->ReadyPlayerCount = 0;
	}

	ReadyPlayers.Empty();
	RestartReadyPlayers.Empty();
	PendingRespawnWeapons.Empty();
	CacheInitialWeaponSpawns();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MatchTimerHandle);
	}
}

//Server-authoritative login gate; rejects when player count reaches limit.
//服务器授权登录闸口；人数达上限时拒绝连接。
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

//Assign team id by player index modulo team count.
//按玩家索引对队伍数取模分配队伍ID。
void AFPSGameModeBase::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	AFPSPlayerState* FPSPlayerState = NewPlayer ? Cast<AFPSPlayerState>(NewPlayer->PlayerState) : nullptr;
	if (!FPSPlayerState || TeamCount <= 0) return;
	const int32 PlayerIndex = GameState ? (GameState->PlayerArray.Num() - 1) : 0;
	const int32 TeamId = PlayerIndex % TeamCount;
	FPSPlayerState->SetTeamId(TeamId);

	UpdateReadyCount();
}

void AFPSGameModeBase::Logout(AController* Exiting)
{
	Super::Logout(Exiting);

	APlayerController* ExitingPC = Cast<APlayerController>(Exiting);
	if (ExitingPC)
	{
		ReadyPlayers.Remove(ExitingPC);
		RestartReadyPlayers.Remove(ExitingPC);
	}

	UpdateReadyCount();

	AFPSGameState* FPSGameState = GetFPSGameState();
	if (!FPSGameState)
	{
		return;
	}

	if (FPSGameState->bWaitingForStart && AreAllPlayersReady(ReadyPlayers))
	{
		BeginMatch();
	}
	else if (FPSGameState->bWaitingForRestart && AreAllPlayersReady(RestartReadyPlayers))
	{
		BeginRestart();
	}
}

//Selects a free PlayerStart by PlayerId-based naming (PlayerStart, PlayerStart1...).
//按 PlayerId 命名的空闲出生点选择（PlayerStart、PlayerStart1...）。
AActor* AFPSGameModeBase::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!Player || !GetWorld()) return Super::ChoosePlayerStart_Implementation(Player);
	int32 PlayerId = 0;
	if (APlayerState* PlayerState = Player->GetPlayerState<APlayerState>())
	{
		PlayerId = PlayerState->GetPlayerId();
	}

	const int32 MaxStartIndex = 128;
	for (int32 Offset = 0; Offset < MaxStartIndex; ++Offset)
	{
		const int32 StartIndex = (PlayerId + Offset) % MaxStartIndex;
		const FString TargetName = (StartIndex == 0) ? TEXT("PlayerStart")
			: FString::Printf(TEXT("PlayerStart%d"), StartIndex);

		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			if (It->GetName() == TargetName)
			{
				if (IsPlayerStartFree(*It))
				{
					return *It;
				}
				break;
			}
		}
	}

	return Super::ChoosePlayerStart_Implementation(Player);
}

//Server-side kill handler: updates kills/deaths, team score, kill feedback.
//服务器击杀处理：更新击杀/死亡、队伍分数及击杀反馈。
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

	if (KillerPS && VictimPS && KillerPS != VictimPS)
	{
		const int32 KillerTeam = KillerPS->TeamId;
		const int32 VictimTeam = VictimPS->TeamId;
		if (KillerTeam != VictimTeam)
		{
			if (AFPSGameState* FPSGameState = GetFPSGameState())
			{
				if (KillerTeam == 0)
				{
					++FPSGameState->TeamAScore;
				}
				else if (KillerTeam == 1)
				{
					++FPSGameState->TeamBScore;
				}
				// TODO: add team system for more players/teams.
			}
		}
	}

	if (KillSound && Killer)
	{
		if (AFPSPlayerController* KillerPC = Cast<AFPSPlayerController>(Killer))
		{
			KillerPC->ClientPlaySound2D(KillSound, KillSoundVolume);
		}
	}

	if (Killer)
	{
		if (AFPSPlayerController* KillerPC = Cast<AFPSPlayerController>(Killer))
		{
			KillerPC->ClientShowKillIcon(KillIconDuration);
		}
	}
}

//Schedules respawn after delay; optionally preserves weapon on respawn.
//延迟后安排重生；可配置重生时保留武器。
void AFPSGameModeBase::RequestRespawn(AController* Controller, APawn* DeadPawn)
{
	if (!Controller) return;
	if (DeadPawn)
	{
		if (bKeepWeaponOnRespawn)
		{
			if (AWeaponBase* Weapon = FindWeaponFromPawn(DeadPawn))
			{
				//Clean widget before hiding weapon to avoid stale UI.
				//隐藏武器前清理 widget 避免残留 UI。
				Weapon->CleanupFireWidget();
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
				//Clean widget before destroy so client also cleans up.
				//销毁前清理 widget，确保客户端同步清理。
				Weapon->CleanupFireWidget();
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

bool AFPSGameModeBase::IsPlayerStartFree(const APlayerStart* Start) const
{
	if (!Start || !GetWorld())
	{
		return false;
	}

	const FVector StartLocation = Start->GetActorLocation();
	const float RadiusSq = SpawnCheckRadius * SpawnCheckRadius;

	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		const APawn* Pawn = *It;
		if (!IsValid(Pawn))
		{
			continue;
		}

		if (FVector::DistSquared(Pawn->GetActorLocation(), StartLocation) <= RadiusSq)
		{
			return false;
		}
	}

	return true;
}

void AFPSGameModeBase::HandleMatchTimerTick()
{
	if (!GetWorld())
	{
		return;
	}

	AFPSGameState* FPSGameState = GetFPSGameState();
	if (!FPSGameState || FPSGameState->bMatchOver || !FPSGameState->bMatchStarted)
	{
		return;
	}

	FPSGameState->RemainingTime = FMath::Max(0, FPSGameState->RemainingTime - 1);
	if (FPSGameState->RemainingTime <= 0)
	{
		EndMatchIfNeeded();
	}
}

void AFPSGameModeBase::EndMatchIfNeeded()
{
	if (!GetWorld())
	{
		return;
	}

	AFPSGameState* FPSGameState = GetFPSGameState();
	if (!FPSGameState || FPSGameState->bMatchOver)
	{
		return;
	}

	FPSGameState->bMatchOver = true;
	FPSGameState->bMatchStarted = false;
	FPSGameState->bWaitingForRestart = true;
	FPSGameState->bWaitingForStart = false;
	FPSGameState->ReadyPlayerCount = 0;
	if (FPSGameState->TeamAScore > FPSGameState->TeamBScore)
	{
		FPSGameState->WinningTeamId = 0;
	}
	else if (FPSGameState->TeamBScore > FPSGameState->TeamAScore)
	{
		FPSGameState->WinningTeamId = 1;
	}
	else
	{
		FPSGameState->WinningTeamId = -1;
	}

	GetWorld()->GetTimerManager().ClearTimer(MatchTimerHandle);

	ReadyPlayers.Empty();
	RestartReadyPlayers.Empty();
}

AFPSGameState* AFPSGameModeBase::GetFPSGameState() const
{
	return GetGameState<AFPSGameState>();
}

//Handles ready button: start match or restart when all players ready.
//处理就绪按钮：全员就绪时开始或重开比赛。
void AFPSGameModeBase::HandlePlayerReady(APlayerController* PlayerController, bool bForRestart)
{
	if (!PlayerController) return;
	AFPSGameState* FPSGameState = GetFPSGameState();
	if (!FPSGameState)
	{
		return;
	}

	if (bForRestart)
	{
		if (!FPSGameState->bWaitingForRestart)
		{
			return;
		}

		if (RestartReadyPlayers.Contains(PlayerController))
		{
			return;
		}

		RestartReadyPlayers.Add(PlayerController);
		UpdateReadyCount();

		if (AreAllPlayersReady(RestartReadyPlayers))
		{
			BeginRestart();
		}
	}
	else
	{
		if (!FPSGameState->bWaitingForStart)
		{
			return;
		}

		if (ReadyPlayers.Contains(PlayerController))
		{
			return;
		}

		ReadyPlayers.Add(PlayerController);
		UpdateReadyCount();

		if (AreAllPlayersReady(ReadyPlayers))
		{
			BeginMatch();
		}
	}
}

void AFPSGameModeBase::StartMatchTimer()
{
	if (!GetWorld() || MatchTimeSeconds <= 0)
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(MatchTimerHandle);
	GetWorld()->GetTimerManager().SetTimer(MatchTimerHandle, this, &AFPSGameModeBase::HandleMatchTimerTick, 1.0f, true);
}

void AFPSGameModeBase::BeginMatch()
{
	AFPSGameState* FPSGameState = GetFPSGameState();
	if (!FPSGameState)
	{
		return;
	}

	FPSGameState->bMatchOver = false;
	FPSGameState->bMatchStarted = true;
	FPSGameState->bWaitingForStart = false;
	FPSGameState->bWaitingForRestart = false;
	FPSGameState->WinningTeamId = -1;
	FPSGameState->ReadyPlayerCount = 0;
	FPSGameState->RemainingTime = MatchTimeSeconds;

	ReadyPlayers.Empty();
	RestartReadyPlayers.Empty();

	StartMatchTimer();
}

void AFPSGameModeBase::BeginRestart()
{
	AFPSGameState* FPSGameState = GetFPSGameState();
	if (!FPSGameState)
	{
		return;
	}

	FPSGameState->TeamAScore = 0;
	FPSGameState->TeamBScore = 0;
	FPSGameState->WinningTeamId = -1;
	FPSGameState->bMatchOver = false;
	FPSGameState->bMatchStarted = true;
	FPSGameState->bWaitingForStart = false;
	FPSGameState->bWaitingForRestart = false;
	FPSGameState->ReadyPlayerCount = 0;
	FPSGameState->RemainingTime = MatchTimeSeconds;

	ReadyPlayers.Empty();
	RestartReadyPlayers.Empty();

	RespawnAllPlayers();
	RespawnAllWeapons();

	StartMatchTimer();
}

void AFPSGameModeBase::UpdateReadyCount()
{
	AFPSGameState* FPSGameState = GetFPSGameState();
	if (!FPSGameState)
	{
		return;
	}

	if (FPSGameState->bWaitingForRestart)
	{
		FPSGameState->ReadyPlayerCount = RestartReadyPlayers.Num();
	}
	else if (FPSGameState->bWaitingForStart)
	{
		FPSGameState->ReadyPlayerCount = ReadyPlayers.Num();
	}
	else
	{
		FPSGameState->ReadyPlayerCount = 0;
	}
}

bool AFPSGameModeBase::AreAllPlayersReady(const TSet<TWeakObjectPtr<APlayerController>>& ReadySet) const
{
	const int32 TotalPlayers = GameState ? GameState->PlayerArray.Num() : 0;
	if (TotalPlayers <= 0)
	{
		return false;
	}

	return ReadySet.Num() >= TotalPlayers;
}

void AFPSGameModeBase::CacheInitialWeaponSpawns()
{
	if (!GetWorld())
	{
		return;
	}

	InitialWeaponSpawns.Empty();
	for (TActorIterator<AWeaponBase> It(GetWorld()); It; ++It)
	{
		AWeaponBase* Weapon = *It;
		if (!IsValid(Weapon))
		{
			continue;
		}

		FWeaponSpawnInfo Info;
		Info.WeaponClass = Weapon->GetClass();
		Info.Transform = Weapon->GetActorTransform();
		InitialWeaponSpawns.Add(Info);
	}
}

void AFPSGameModeBase::RespawnAllWeapons()
{
	if (!GetWorld())
	{
		return;
	}

	//Clean all weapon widgets before destroy to avoid last-round residue.
	//销毁前清理所有武器 widget 防止上局残留。
	for (TActorIterator<AWeaponBase> It(GetWorld()); It; ++It)
	{
		if (AWeaponBase* Weapon = *It)
		{
			Weapon->CleanupFireWidget();
			Weapon->Destroy();
		}
	}

	for (const FWeaponSpawnInfo& Info : InitialWeaponSpawns)
	{
		if (!Info.WeaponClass)
		{
			continue;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		GetWorld()->SpawnActor<AWeaponBase>(Info.WeaponClass, Info.Transform, SpawnParams);
	}
}

void AFPSGameModeBase::RespawnAllPlayers()
{
	if (!GetWorld())
	{
		return;
	}

	PendingRespawnWeapons.Empty();

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		if (APawn* Pawn = PC->GetPawn())
		{
			Pawn->Destroy();
		}

		RestartPlayer(PC);
	}
}


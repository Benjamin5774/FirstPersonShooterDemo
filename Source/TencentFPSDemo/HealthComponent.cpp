#include "HealthComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "FPSGameModeBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	MaxHealth = 100.0f;
	Health = MaxHealth;
	HitEffectDuration = 0.2f;
	ActiveHitWidget = nullptr;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	OnHealthChanged.Broadcast(Health, MaxHealth);
	RemoveHitEffect();
}

//Server-authoritative damage entry; triggers hit effect RPC and death handling.
//服务器授权伤害入口；触发受击红屏 RPC 及死亡处理。
void UHealthComponent::ApplyDamage(float Amount, AController* InstigatorController)
{
	if (!GetOwner() || !GetOwner()->HasAuthority()) return;
	if (Health <= 0.0f || Amount <= 0.0f)
	{
		return;
	}

	Health = FMath::Clamp(Health - Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
			ClientShowHitEffect(HitEffectDuration);
	}

	if (Health <= 0.0f)
	{
		HandleDeath(InstigatorController);
	}
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, MaxHealth);
}

//Disables input/collision and notifies GameMode for respawn.
//禁用输入/碰撞并通知 GameMode 处理重生。
void UHealthComponent::HandleDeath(AController* InstigatorController)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter) return;
	RemoveHitEffect();
	if (UWorld* World = GetWorld())
	{
		if (AFPSGameModeBase* GameMode = World->GetAuthGameMode<AFPSGameModeBase>())
		{
			GameMode->OnPlayerKilled(InstigatorController, OwnerCharacter->GetController());
			GameMode->RequestRespawn(OwnerCharacter->GetController(), OwnerCharacter);
		}
	}

	OwnerCharacter->DisableInput(nullptr);
	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}

	OwnerCharacter->SetActorEnableCollision(false);
}

void UHealthComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveHitEffect();
	Super::EndPlay(EndPlayReason);
}

//Client RPC: show hit effect overlay for duration.
//客户端 RPC：显示指定时长的受击红屏叠加层。
void UHealthComponent::ClientShowHitEffect_Implementation(float Duration)
{
	if (!HitEffectWidgetClass) return;
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PC) return;
	RemoveHitEffect();
	ActiveHitWidget = CreateWidget<UUserWidget>(PC, HitEffectWidgetClass);
	if (ActiveHitWidget) ActiveHitWidget->AddToViewport();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitEffectTimerHandle);
		World->GetTimerManager().SetTimer(HitEffectTimerHandle, this, &UHealthComponent::RemoveHitEffect,
			FMath::Max(0.01f, Duration), false);
	}
}

void UHealthComponent::RemoveHitEffect()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitEffectTimerHandle);
	}

	if (ActiveHitWidget)
	{
		ActiveHitWidget->RemoveFromParent();
		ActiveHitWidget = nullptr;
	}
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHealthComponent, Health);
}


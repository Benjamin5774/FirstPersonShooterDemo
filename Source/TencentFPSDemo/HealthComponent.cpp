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
}

void UHealthComponent::ApplyDamage(float Amount, AController* InstigatorController)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("ApplyDamage进入：Owner=%s Amount=%.2f"),
		*GetOwner()->GetName(), Amount);

	if (Health <= 0.0f || Amount <= 0.0f)
	{
		return;
	}

	Health = FMath::Clamp(Health - Amount, 0.0f, MaxHealth);
	OnHealthChanged.Broadcast(Health, MaxHealth);

	if (ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController()))
		{
			UE_LOG(LogTemp, Log, TEXT("服务器触发受击红屏RPC"));
			ClientShowHitEffect(HitEffectDuration);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("服务器触发受击红屏RPC失败：无PlayerController"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("服务器触发受击红屏RPC失败：Owner不是Character"));
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

void UHealthComponent::HandleDeath(AController* InstigatorController)
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

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

void UHealthComponent::ClientShowHitEffect_Implementation(float Duration)
{
	UE_LOG(LogTemp, Log, TEXT("受击已调用红色屏幕"));
	if (!HitEffectWidgetClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("受击但调用红色屏幕失败"));
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APlayerController* PC = OwnerPawn ? Cast<APlayerController>(OwnerPawn->GetController()) : nullptr;
	if (!PC)
	{
		UE_LOG(LogTemp, Warning, TEXT("受击但调用红色屏幕失败"));
		return;
	}

	RemoveHitEffect();

	ActiveHitWidget = CreateWidget<UUserWidget>(PC, HitEffectWidgetClass);
	if (ActiveHitWidget)
	{
		ActiveHitWidget->AddToViewport();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("受击但调用红色屏幕失败"));
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HitEffectTimerHandle);
		World->GetTimerManager().SetTimer(HitEffectTimerHandle, this, &UHealthComponent::RemoveHitEffect,
			FMath::Max(0.01f, Duration), false);
	}
}

void UHealthComponent::RemoveHitEffect()
{
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


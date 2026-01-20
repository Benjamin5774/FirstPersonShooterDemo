#include "HealthComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "FPSGameModeBase.h"
#include "Net/UnrealNetwork.h"

UHealthComponent::UHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	MaxHealth = 100.0f;
	Health = MaxHealth;
}

void UHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
}

void UHealthComponent::ApplyDamage(float Amount, AController* InstigatorController)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (Health <= 0.0f || Amount <= 0.0f)
	{
		return;
	}

	Health = FMath::Clamp(Health - Amount, 0.0f, MaxHealth);

	if (Health <= 0.0f)
	{
		HandleDeath(InstigatorController);
	}
}

void UHealthComponent::OnRep_Health(float OldHealth)
{
	// Placeholder for hit/death UI feedback.
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
		}
	}

	OwnerCharacter->DisableInput(nullptr);
	if (UCharacterMovementComponent* MoveComp = OwnerCharacter->GetCharacterMovement())
	{
		MoveComp->DisableMovement();
	}

	OwnerCharacter->SetActorEnableCollision(false);
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UHealthComponent, Health);
}


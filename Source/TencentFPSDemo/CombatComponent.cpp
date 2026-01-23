#include "CombatComponent.h"
#include "HealthComponent.h"
#include "FPSPlayerState.h"
#include "GameFramework/Pawn.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	TraceDistance = 10000.0f;
	Damage = 25.0f;
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCombatComponent::StartFire()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	if (OwnerPawn->IsLocallyControlled())
	{
		FVector EyeLocation;
		FRotator EyeRotation;
		OwnerPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		ServerFire(EyeLocation, EyeRotation.Vector());
	}
}

void UCombatComponent::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir)
{
	if (Dir.IsNearlyZero())
	{
		return;
	}

	HandleLineTrace(Origin, Dir);
}

void UCombatComponent::HandleLineTrace(const FVector& Origin, const FVector& Dir)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FVector TraceDir = Dir.GetSafeNormal();
	FVector End = Origin + (TraceDir * TraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CombatTrace), true);
	Params.AddIgnoredActor(GetOwner());

	FHitResult HitResult;
	bool bHit = World->LineTraceSingleByChannel(HitResult, Origin, End, ECC_Visibility, Params);
	if (!bHit)
	{
		return;
	}

	AActor* HitActor = HitResult.GetActor();
	if (!HitActor)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	APawn* HitPawn = Cast<APawn>(HitActor);
	if (OwnerPawn && HitPawn)
	{
		AFPSPlayerState* OwnerPS = Cast<AFPSPlayerState>(OwnerPawn->GetPlayerState());
		AFPSPlayerState* HitPS = Cast<AFPSPlayerState>(HitPawn->GetPlayerState());
		if (OwnerPS && HitPS && OwnerPS->TeamId == HitPS->TeamId)
		{
			return;
		}
	}

	if (UHealthComponent* HealthComp = HitActor->FindComponentByClass<UHealthComponent>())
	{
		AController* InstigatorController = OwnerPawn ? OwnerPawn->GetController() : nullptr;
		HealthComp->ApplyDamage(Damage, InstigatorController);
	}
}


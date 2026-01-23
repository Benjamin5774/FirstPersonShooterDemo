#include "CombatComponent.h"
#include "DamageEffectInterface.h"
#include "HealthComponent.h"
#include "FPSPlayerState.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);

	TraceDistance = 10000.0f;
	Damage = 25.0f;
	HitTextColor = FLinearColor::Red;
	HitEffectZOffset = 60.0f;
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

	UE_LOG(LogTemp, Log, TEXT("CombatComponent StartFire: Owner=%s LocallyControlled=%d"), *GetNameSafe(OwnerPawn), OwnerPawn->IsLocallyControlled());
	if (OwnerPawn->IsLocallyControlled())
	{
		FVector EyeLocation;
		FRotator EyeRotation;
		OwnerPawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);

		UE_LOG(LogTemp, Log, TEXT("CombatComponent StartFire -> ServerFire"));
		ServerFire(EyeLocation, EyeRotation.Vector());
	}
}

void UCombatComponent::ServerFire_Implementation(FVector_NetQuantize Origin, FVector_NetQuantizeNormal Dir)
{
	UE_LOG(LogTemp, Log, TEXT("CombatComponent ServerFire: Origin=%s Dir=%s"), *FVector(Origin).ToString(), *FVector(Dir).ToString());
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

	TArray<FHitResult> HitResults;
	bool bHit = World->LineTraceMultiByChannel(HitResults, Origin, End, ECC_Visibility, Params);
	if (!bHit)
	{
		UE_LOG(LogTemp, Log, TEXT("CombatComponent Trace: NoHit"));
		return;
	}

	FHitResult HitResult;
	AActor* HitActor = nullptr;
	for (const FHitResult& Candidate : HitResults)
	{
		AActor* CandidateActor = Candidate.GetActor();
		if (!CandidateActor)
		{
			continue;
		}

		HitResult = Candidate;
		HitActor = CandidateActor;
		break;
	}

	if (!HitActor)
	{
		UE_LOG(LogTemp, Log, TEXT("CombatComponent Trace: NoValidHit"));
		return;
	}
	UE_LOG(LogTemp, Log, TEXT("CombatComponent Trace: HitActor=%s"), *GetNameSafe(HitActor));

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
		UE_LOG(LogTemp, Log, TEXT("受击效果开始播放"));
		ClientShowHitFeedback(HitResult.ImpactPoint, Damage);
	}
}

void UCombatComponent::ClientShowHitFeedback_Implementation(FVector_NetQuantize Location, float DamageAmount)
{
	UE_LOG(LogTemp, Log, TEXT("受击效果RPC: Location=%s Damage=%.2f"), *FVector(Location).ToString(), DamageAmount);
	const FVector SpawnLocation = FVector(Location) + FVector(0.0f, 0.0f, HitEffectZOffset);

	if (HitSound)
	{
		UE_LOG(LogTemp, Log, TEXT("受击声音开始播放"));
		UGameplayStatics::PlaySoundAtLocation(this, HitSound, SpawnLocation);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("受击音效为空: HitSound 未设置"));
	}

	if (!HitEffectClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("受击特效为空: HitEffectClass 未设置"));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* EffectActor = World->SpawnActor<AActor>(HitEffectClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);
	if (!EffectActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("受击特效生成失败: 类=%s"), *GetNameSafe(HitEffectClass));
		return;
	}
	if (EffectActor && EffectActor->GetClass()->ImplementsInterface(UDamageEffectInterface::StaticClass()))
	{
		IDamageEffectInterface::Execute_InitDamageEffect(EffectActor, DamageAmount, HitTextColor);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("受击特效未实现接口: %s"), *GetNameSafe(EffectActor));
	}
}


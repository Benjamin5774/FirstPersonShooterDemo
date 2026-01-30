#include "MovementSoundComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

UMovementSoundComponent::UMovementSoundComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(false);

	VolumeMultiplier = 1.0f;
	MinSpeedToPlay = 10.0f;
	MaxSpeed = 600.0f;
	MinPitch = 0.9f;
	MaxPitch = 1.6f;

	MoveAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MoveAudioComponent"));
	if (MoveAudioComponent)
	{
		MoveAudioComponent->bAutoActivate = false;
		MoveAudioComponent->bIsUISound = false;
	}
}

void UMovementSoundComponent::BeginPlay()
{
	Super::BeginPlay();

	if (MoveAudioComponent)
	{
		if (AActor* Owner = GetOwner())
		{
			MoveAudioComponent->SetupAttachment(Owner->GetRootComponent());
		}

		if (!MoveAudioComponent->IsRegistered())
		{
			MoveAudioComponent->RegisterComponent();
		}
	}
}

void UMovementSoundComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (MoveAudioComponent)
	{
		MoveAudioComponent->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

void UMovementSoundComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!MoveSound || !ShouldPlayForOwner())
	{
		if (MoveAudioComponent && MoveAudioComponent->IsPlaying())
		{
			MoveAudioComponent->Stop();
		}
		return;
	}

	UpdateMovementAudio(GetOwnerSpeed2D());
}

//Play movement sound only for locally controlled pawn (avoids duplicate audio).
//仅对本地控制 Pawn 播放移动音效（避免重复）。
bool UMovementSoundComponent::ShouldPlayForOwner() const
{
	const APawn* PawnOwner = Cast<APawn>(GetOwner());
	if (!PawnOwner)
	{
		return false;
	}

	return PawnOwner->IsLocallyControlled();
}

float UMovementSoundComponent::GetOwnerSpeed2D() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return 0.0f;
	}

	if (const ACharacter* CharacterOwner = Cast<ACharacter>(OwnerActor))
	{
		if (const UCharacterMovementComponent* MoveComp = CharacterOwner->GetCharacterMovement())
		{
			return MoveComp->Velocity.Size2D();
		}
	}

	return OwnerActor->GetVelocity().Size2D();
}

//Pitch scales with speed (MinSpeedToPlay..MaxSpeed -> MinPitch..MaxPitch).
//Pitch 随速度缩放（MinSpeedToPlay..MaxSpeed 映射到 MinPitch..MaxPitch）。
void UMovementSoundComponent::UpdateMovementAudio(float Speed2D)
{
	if (!MoveAudioComponent)
	{
		return;
	}

	if (Speed2D <= MinSpeedToPlay)
	{
		if (MoveAudioComponent->IsPlaying())
		{
			MoveAudioComponent->Stop();
		}
		return;
	}

	MoveAudioComponent->SetSound(MoveSound);

	const float ClampedSpeed = FMath::Clamp(Speed2D, MinSpeedToPlay, MaxSpeed);
	const float Alpha = (MaxSpeed > MinSpeedToPlay)
		? (ClampedSpeed - MinSpeedToPlay) / (MaxSpeed - MinSpeedToPlay)
		: 1.0f;
	const float Pitch = FMath::Lerp(MinPitch, MaxPitch, Alpha);

	MoveAudioComponent->SetPitchMultiplier(Pitch);
	MoveAudioComponent->SetVolumeMultiplier(VolumeMultiplier);

	if (!MoveAudioComponent->IsPlaying())
	{
		MoveAudioComponent->Play();
	}
}


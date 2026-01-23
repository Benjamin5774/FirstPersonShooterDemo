#include "DamageNumberActor.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Net/UnrealNetwork.h"
#include "Blueprint/WidgetTree.h"

ADamageNumberActor::ADamageNumberActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bOnlyRelevantToOwner = true;
	SetReplicateMovement(false);
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
	WidgetComponent->SetupAttachment(RootComponent);
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WidgetComponent->SetGenerateOverlapEvents(false);

	RiseSpeed = 40.0f;
	LifeTime = 1.0f;
	SpawnOffset = FVector(0.0f, 0.0f, 50.0f);
	bUseScreenSpace = true;
	DamageValue = 0.0f;
	DamageColor = FLinearColor::White;
	ElapsedTime = 0.0f;
}

void ADamageNumberActor::BeginPlay()
{
	Super::BeginPlay();

	if (DamageWidgetClass)
	{
		WidgetComponent->SetWidgetClass(DamageWidgetClass);
		WidgetComponent->InitWidget();
	}

	WidgetComponent->SetWidgetSpace(bUseScreenSpace ? EWidgetSpace::Screen : EWidgetSpace::World);
	WidgetComponent->SetDrawAtDesiredSize(true);

	if (!SpawnOffset.IsNearlyZero())
	{
		AddActorWorldOffset(SpawnOffset, false);
	}

	ApplyToWidget();
}

void ADamageNumberActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	ElapsedTime += DeltaSeconds;
	if (ElapsedTime >= LifeTime)
	{
		Destroy();
		return;
	}

	if (!FMath::IsNearlyZero(RiseSpeed))
	{
		const FVector Offset(0.0f, 0.0f, RiseSpeed * DeltaSeconds);
		AddActorWorldOffset(Offset, false);
	}
}

void ADamageNumberActor::InitDamageEffect_Implementation(float Damage, FLinearColor Color)
{
	DamageValue = Damage;
	DamageColor = Color;
	ApplyToWidget();
}

void ADamageNumberActor::OnRep_DamageInfo()
{
	ApplyToWidget();
}

void ADamageNumberActor::ApplyToWidget()
{
	if (!WidgetComponent)
	{
		return;
	}

	UUserWidget* Widget = WidgetComponent->GetUserWidgetObject();
	if (!Widget)
	{
		return;
	}

	if (UWidgetTree* WidgetTree = Widget->WidgetTree)
	{
		if (UTextBlock* DamageText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("DamageText"))))
		{
			DamageText->SetText(FText::AsNumber(FMath::RoundToInt(DamageValue)));
			DamageText->SetColorAndOpacity(FSlateColor(DamageColor));
		}
	}
}

void ADamageNumberActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADamageNumberActor, DamageValue);
	DOREPLIFETIME(ADamageNumberActor, DamageColor);
}

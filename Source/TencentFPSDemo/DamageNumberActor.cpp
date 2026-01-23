#include "DamageNumberActor.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetComponent.h"

ADamageNumberActor::ADamageNumberActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = false;
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	WidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("WidgetComponent"));
	WidgetComponent->SetupAttachment(RootComponent);
	WidgetComponent->SetWidgetSpace(EWidgetSpace::World);
	WidgetComponent->SetDrawAtDesiredSize(true);
	WidgetComponent->SetTwoSided(true);
	WidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RiseSpeed = 40.0f;
	LifeTime = 1.0f;
}

void ADamageNumberActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Log, TEXT("受击数字Actor BeginPlay: %s"), *GetName());

	if (DamageWidgetClass)
	{
		WidgetComponent->SetWidgetClass(DamageWidgetClass);
		WidgetComponent->InitWidget();
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("受击数字Actor: DamageWidgetClass 未设置"));
	}

	if (LifeTime > 0.0f)
	{
		SetLifeSpan(LifeTime);
	}
}

void ADamageNumberActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (RiseSpeed != 0.0f)
	{
		AddActorWorldOffset(FVector(0.0f, 0.0f, RiseSpeed * DeltaSeconds));
	}
}

void ADamageNumberActor::InitDamageEffect_Implementation(float Damage, FLinearColor Color)
{
	if (!WidgetComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("受击数字Actor: WidgetComponent 为空"));
		return;
	}

	UUserWidget* UserWidget = WidgetComponent->GetUserWidgetObject();
	if (!UserWidget && DamageWidgetClass)
	{
		WidgetComponent->SetWidgetClass(DamageWidgetClass);
		WidgetComponent->InitWidget();
		UserWidget = WidgetComponent->GetUserWidgetObject();
	}

	if (!UserWidget)
	{
		UE_LOG(LogTemp, Warning, TEXT("受击数字Actor: UserWidget 为空"));
		return;
	}

	UTextBlock* DamageTextBlock = Cast<UTextBlock>(UserWidget->GetWidgetFromName(TEXT("DamageText")));
	if (!DamageTextBlock)
	{
		UE_LOG(LogTemp, Warning, TEXT("受击数字Actor: 未找到 DamageText 文本"));
		return;
	}

	const FText DamageText = FText::Format(NSLOCTEXT("DamageEffect", "DamageValue", "-{0}"), FText::AsNumber(Damage));
	DamageTextBlock->SetText(DamageText);
	DamageTextBlock->SetColorAndOpacity(FSlateColor(Color));
}


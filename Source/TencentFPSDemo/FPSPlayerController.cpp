#include "FPSPlayerController.h"
#include "FPSGameModeBase.h"
#include "FPSHUD.h"
#include "WeaponBase.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"
#include "Kismet/GameplayStatics.h"
#include "Components/InputComponent.h"
#include "EngineUtils.h"

//Server RPC: mark player ready for match start.
//服务器 RPC：标记玩家准备开始比赛。
void AFPSPlayerController::ServerSetReadyForStart_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AFPSGameModeBase* GameMode = World->GetAuthGameMode<AFPSGameModeBase>())
		{
			GameMode->HandlePlayerReady(this, false);
		}
	}
}

void AFPSPlayerController::ServerSetReadyForRestart_Implementation()
{
	if (UWorld* World = GetWorld())
	{
		if (AFPSGameModeBase* GameMode = World->GetAuthGameMode<AFPSGameModeBase>())
		{
			GameMode->HandlePlayerReady(this, true);
		}
	}
}

void AFPSPlayerController::ClientPlaySound2D_Implementation(USoundBase* Sound, float Volume)
{
	if (!Sound)
	{
		return;
	}

	UGameplayStatics::PlaySound2D(this, Sound, Volume);
}

void AFPSPlayerController::ClientShowKillIcon_Implementation(float Duration)
{
	if (AFPSHUD* HUD = Cast<AFPSHUD>(GetHUD()))
	{
		HUD->ShowKillIcon(Duration);
	}
}

//Reload key defaults to E; ADS key to RightMouseButton.
//换弹键默认 E；开镜键默认右键。
AFPSPlayerController::AFPSPlayerController()
{
	ReloadKey = EKeys::E;
}

void AFPSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	if (InputComponent)
	{
		InputComponent->BindKey(ReloadKey, IE_Pressed, this, &AFPSPlayerController::OnReloadPressed);
		InputComponent->BindKey(ADSKey, IE_Pressed, this, &AFPSPlayerController::OnADSPressed);
		InputComponent->BindKey(ADSKey, IE_Released, this, &AFPSPlayerController::OnADSReleased);
	}
}

void AFPSPlayerController::OnADSPressed()
{
	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->SetADS(true);
	}
}

void AFPSPlayerController::OnADSReleased()
{
	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->SetADS(false);
	}
}

void AFPSPlayerController::OnReloadPressed()
{
	if (AWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->StartReload();
	}
}

AWeaponBase* AFPSPlayerController::GetCurrentWeapon() const
{
	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return nullptr;
	}

	for (TFieldIterator<FProperty> PropIt(ControlledPawn->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
		{
			if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(AWeaponBase::StaticClass()))
			{
				UObject* WeaponObj = ObjProp->GetObjectPropertyValue_InContainer(ControlledPawn);
				if (AWeaponBase* Weapon = Cast<AWeaponBase>(WeaponObj))
				{
					if (Weapon->GetOwner() == ControlledPawn)
					{
						return Weapon;
					}
				}
			}
		}
	}

	TArray<FName> PossibleWeaponNames = { 
		TEXT("CurrentWeapon"), 
		TEXT("Weapon"), 
		TEXT("EquippedWeapon"),
		TEXT("MyWeapon"),
		TEXT("PrimaryWeapon")
	};

	for (const FName& WeaponVarName : PossibleWeaponNames)
	{
		FProperty* Prop = ControlledPawn->GetClass()->FindPropertyByName(WeaponVarName);
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
		{
			if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(AWeaponBase::StaticClass()))
			{
				UObject* WeaponObj = ObjProp->GetObjectPropertyValue_InContainer(ControlledPawn);
				if (AWeaponBase* Weapon = Cast<AWeaponBase>(WeaponObj))
				{
					if (Weapon->GetOwner() == ControlledPawn)
					{
						return Weapon;
					}
				}
			}
		}
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AWeaponBase> It(World); It; ++It)
		{
			if (AWeaponBase* Weapon = *It)
			{
				if (Weapon->GetOwner() == ControlledPawn)
				{
					return Weapon;
				}
			}
		}
	}

	return nullptr;
}

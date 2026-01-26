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

AFPSPlayerController::AFPSPlayerController()
{
	// 默认换弹键为E
	ReloadKey = EKeys::E;
}

void AFPSPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// 绑定换弹键（使用FKey直接绑定）
	// 在UE5中，PlayerController有InputComponent成员变量，可以直接访问
	if (InputComponent)
	{
		// 使用BindKey直接绑定按键
		InputComponent->BindKey(ReloadKey, IE_Pressed, this, &AFPSPlayerController::OnReloadPressed);
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

	// 方法1: 通过反射查找所有AWeaponBase类型的属性
	// 这样可以自动找到武器，无论变量名是什么
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
					// 验证武器确实属于这个Pawn
					if (Weapon->GetOwner() == ControlledPawn)
					{
						return Weapon;
					}
				}
			}
		}
	}

	// 方法2: 如果反射没找到，尝试常见的武器变量名
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

	// 方法3: 作为最后的手段，遍历所有武器，找到owner是当前Pawn的武器
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


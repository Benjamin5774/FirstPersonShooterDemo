#include "WeaponBase.h"
#include "HealthComponent.h"
#include "FPSPlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextBlock.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"
#include "TimerManager.h"
#include "EngineUtils.h"

namespace
{
	bool GetMeshSocketOrBoneTransform(USkeletalMeshComponent* Mesh, const FName& Name, ERelativeTransformSpace Space, FTransform& OutTransform)
	{
		if (!Mesh || Name.IsNone())
		{
			return false;
		}

		if (Mesh->DoesSocketExist(Name))
		{
			OutTransform = Mesh->GetSocketTransform(Name, Space);
			return true;
		}

		const int32 BoneIndex = Mesh->GetBoneIndex(Name);
		if (BoneIndex == INDEX_NONE)
		{
			return false;
		}

		FTransform BoneInComponent = Mesh->GetBoneTransform(BoneIndex);
		if (Space == RTS_Component)
		{
			OutTransform = BoneInComponent;
			return true;
		}

		if (Space == RTS_World)
		{
			OutTransform = BoneInComponent * Mesh->GetComponentTransform();
			return true;
		}

		OutTransform = BoneInComponent;
		return true;
	}
}

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	SetRootComponent(WeaponMesh);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetIsReplicated(true);

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	PickupSphere->SetupAttachment(RootComponent);
	PickupSphere->InitSphereRadius(60.0f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	TraceDistance = 10000.0f;
	Damage = 25.0f;
	FireRate = 8.0f;
	FireSoundVolume = 1.0f;
	MaxAmmo = 30;
	CurrentAmmo = MaxAmmo;
	ReloadTime = 1.2f;
	bIsReloading = false;
	bWantsToFire = false;
	bIsFiring = false;
	bAutoReload = true;
	ReloadEndTime = 0.0f;
	CrosshairResetTime = 0.2f;
	ADSAlpha = 0.0f;
	bIsADS = false;
	// ADSRelativeOffset 默认：枪向相机方向靠一点（按常见第一人称：前向为 X，可设为负 X + 小旋转）
	ADSRelativeOffset.SetLocation(FVector(-15.0f, 0.0f, -5.0f));
	ADSRelativeOffset.SetRotation(FQuat(FRotator(-5.0f, 0.0f, 0.0f)));
	ADSRelativeOffset.SetScale3D(FVector::OneVector);
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (PickupSphere)
	{
		PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::OnPickupSphereBeginOverlap);
	}

	CurrentAmmo = MaxAmmo;
	UpdateAmmoUI();
	
	// 只有在武器已经有owner且owner是本地控制的pawn时才创建widget
	// 这样可以避免重新开始游戏时创建上一局游戏的widget
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsLocallyControlled())
	{
		StartFireWidgetRetry();
		EnsureCrosshairWidget();
	}
	else
	{
		// 确保没有残留的widget
		DestroyFireWidget();
	}
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 在武器被销毁前，确保清理widget
	DestroyFireWidget();
	
	Super::EndPlay(EndPlayReason);
}

void AWeaponBase::OnRep_Owner()
{
	Super::OnRep_Owner();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		SetWeaponVariableOnPawn(OwnerPawn);
		StartFireWidgetRetry();
		EnsureCrosshairWidget();
	}
	else
	{
		// Owner被移除时，清理widget（客户端也会收到这个通知）
		DestroyFireWidget();
	}
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		bHipTransformCaptured = false; // 失去 Owner 时重置，下次装备时重新捕获
		return;
	}
	if (OwnerPawn->IsLocallyControlled())
	{
		UpdateADSTransform(DeltaTime);
	}
}

void AWeaponBase::SetADS(bool bAiming)
{
	bIsADS = bAiming;
}

void AWeaponBase::UpdateADSTransform(float DeltaTime)
{
	USceneComponent* Root = GetRootComponent();
	if (!Root)
	{
		return;
	}

	// 客户端不会执行 AttachToPawn，HipTransform 未设置；用当前复制下来的相对变换作为腰射姿态
	if (!bHipTransformCaptured)
	{
		HipTransform = Root->GetRelativeTransform();
		bHipTransformCaptured = true;
	}

	const float TargetAlpha = bIsADS ? 1.0f : 0.0f;
	ADSAlpha = FMath::FInterpTo(ADSAlpha, TargetAlpha, DeltaTime, ADSSpeed);

	const FTransform ADSTransform = HipTransform * ADSRelativeOffset;
	FTransform CurrentTransform;
	CurrentTransform.SetTranslation(FMath::Lerp(HipTransform.GetTranslation(), ADSTransform.GetTranslation(), ADSAlpha));
	CurrentTransform.SetRotation(FQuat::Slerp(HipTransform.GetRotation(), ADSTransform.GetRotation(), ADSAlpha));
	CurrentTransform.SetScale3D(FMath::Lerp(HipTransform.GetScale3D(), ADSTransform.GetScale3D(), ADSAlpha));
	SetActorRelativeTransform(CurrentTransform, false, nullptr, ETeleportType::None);
}

void AWeaponBase::Fire()
{
	bIsFiring = false;

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	if (!CanFire())
	{
		if (bAutoReload && CurrentAmmo <= 0)
		{
			StartReload();
		}
		return;
	}

	// 有子弹且通过 CanFire，本帧会打出子弹
	bIsFiring = true;

	if (OwnerPawn->IsLocallyControlled())
	{
		if (FireSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation(), FireSoundVolume);
		}
		ApplyRecoil();
	}

	if (HasAuthority())
	{
		ServerFire();
		return;
	}

	if (OwnerPawn->IsLocallyControlled())
	{
		ServerFire();
	}
}

void AWeaponBase::ApplyRecoil()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return;
	}

	// 视角后坐力：准星上抬（负 Pitch）、可选水平偏移；相机震动由蓝图实现
	if (RecoilPitch > 0.0f || RecoilYaw != 0.0f)
	{
		PC->AddPitchInput(-RecoilPitch);
		PC->AddYawInput(RecoilYaw);
	}
}

void AWeaponBase::StartFire()
{
	bWantsToFire = true;
	if (bIsReloading)
	{
		return;
	}

	HandleAutoFireTick();

	const float Interval = GetFireInterval();
	if (Interval > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(FireTimerHandle, this, &AWeaponBase::HandleAutoFireTick, Interval, true);
		}
	}
}

void AWeaponBase::StopFire()
{
	bWantsToFire = false;
	bIsFiring = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	}
}

void AWeaponBase::StartReload()
{
	if (HasAuthority())
	{
		StartReloadInternal(false); // 玩家主动按键填弹
		return;
	}

	ServerStartReload();
}

void AWeaponBase::SetFireWidget(UUserWidget* InWidget)
{
	FireWidget = InWidget;
	UpdateAmmoUI();
	UpdateReloadUI();
}

void AWeaponBase::SetCrosshairState(ECrosshairState State)
{
	SetCrosshairStateInternal(State);
	if (State != ECrosshairState::Default && CrosshairResetTime > 0.0f)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(CrosshairResetTimerHandle);
			World->GetTimerManager().SetTimer(CrosshairResetTimerHandle, this, &AWeaponBase::ResetCrosshairToDefault, CrosshairResetTime, false);
		}
	}
}

void AWeaponBase::SetCrosshairStateInternal(ECrosshairState State)
{
	// 若请求的准星未配置，则保持默认
	if (State == ECrosshairState::Hit && !CrosshairHitWidget)
	{
		State = ECrosshairState::Default;
	}
	if (State == ECrosshairState::Headshot && !CrosshairHeadshotWidget)
	{
		State = ECrosshairState::Default;
	}

	auto SetVisibility = [](UUserWidget* W, bool bVisible)
	{
		if (W)
		{
			W->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};
	SetVisibility(CrosshairDefaultWidget, State == ECrosshairState::Default);
	SetVisibility(CrosshairHitWidget, State == ECrosshairState::Hit);
	SetVisibility(CrosshairHeadshotWidget, State == ECrosshairState::Headshot);
}

void AWeaponBase::ResetCrosshairToDefault()
{
	SetCrosshairStateInternal(ECrosshairState::Default);
}

void AWeaponBase::ClientSetCrosshairState_Implementation(ECrosshairState State)
{
	SetCrosshairState(State);
}

AWeaponBase* AWeaponBase::GetWeaponFromPawn(APawn* Pawn)
{
	if (!Pawn)
	{
		return nullptr;
	}
	for (TFieldIterator<FProperty> PropIt(Pawn->GetClass()); PropIt; ++PropIt)
	{
		FProperty* Prop = *PropIt;
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
		{
			if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(AWeaponBase::StaticClass()))
			{
				UObject* WeaponObj = ObjProp->GetObjectPropertyValue_InContainer(Pawn);
				if (AWeaponBase* Weapon = Cast<AWeaponBase>(WeaponObj))
				{
					if (Weapon->GetOwner() == Pawn)
					{
						return Weapon;
					}
				}
			}
		}
	}
	TArray<FName> PossibleWeaponNames = { TEXT("CurrentWeapon"), TEXT("Weapon"), TEXT("EquippedWeapon"), TEXT("MyWeapon"), TEXT("PrimaryWeapon") };
	for (const FName& WeaponVarName : PossibleWeaponNames)
	{
		FProperty* Prop = Pawn->GetClass()->FindPropertyByName(WeaponVarName);
		if (FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop))
		{
			if (ObjProp->PropertyClass && ObjProp->PropertyClass->IsChildOf(AWeaponBase::StaticClass()))
			{
				UObject* WeaponObj = ObjProp->GetObjectPropertyValue_InContainer(Pawn);
				if (AWeaponBase* Weapon = Cast<AWeaponBase>(WeaponObj))
				{
					if (Weapon->GetOwner() == Pawn)
					{
						return Weapon;
					}
				}
			}
		}
	}
	if (UWorld* World = Pawn->GetWorld())
	{
		for (TActorIterator<AWeaponBase> It(World); It; ++It)
		{
			AWeaponBase* Weapon = *It;
			if (Weapon->GetOwner() == Pawn)
			{
				return Weapon;
			}
		}
	}
	return nullptr;
}

bool AWeaponBase::EquipToPawn(APawn* InPawn)
{
	if (!HasAuthority() || !InPawn)
	{
		return false;
	}

	USkeletalMeshComponent* PawnMesh = FindAttachMeshOnPawn(InPawn);
	if (!PawnMesh)
	{
		return false;
	}

	// 在设置owner前，先清理可能存在的旧widget
	DestroyFireWidget();

	SetOwner(InPawn);
	AttachToPawn(InPawn, PawnMesh);
	AssignWeaponToPawn(InPawn);

	// 玩家装备武器后，创建widget
	StartFireWidgetRetry();

	if (PickupSphere)
	{
		PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	return true;
}

void AWeaponBase::ServerFire_Implementation()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !WeaponMesh)
	{
		return;
	}

	if (!CanFire())
	{
		if (bAutoReload && CurrentAmmo <= 0)
		{
			StartReloadInternal(true); // 无弹时触发的自动填弹，填完后不自动开火
		}
		return;
	}

	ConsumeAmmo();

	const FVector MuzzleLocation = GetMuzzleLocation();
	const FRotator AimRotation = GetAimRotation();
	const FVector AimDir = AimRotation.Vector().GetSafeNormal();

	if (BulletClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerPawn;
		SpawnParams.Instigator = OwnerPawn;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* SpawnedBullet = GetWorld()->SpawnActor<AActor>(BulletClass, MuzzleLocation, AimRotation, SpawnParams);
		if (SpawnedBullet)
		{
			// 设置子弹生命周期，5秒后自动销毁（防止子弹永远不消失）
			SpawnedBullet->SetLifeSpan(5.0f);
		}
	}

	if (!AimDir.IsNearlyZero())
	{
		HandleLineTrace(MuzzleLocation, AimDir);
	}
}

void AWeaponBase::ServerStartReload_Implementation()
{
	StartReloadInternal(false); // 玩家主动按键填弹，填完后可恢复开火
}

void AWeaponBase::OnPickupSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!HasAuthority() || !OtherActor || OtherActor == this)
	{
		return;
	}

	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!Pawn)
	{
		return;
	}

	TryPickup(Pawn);
}

bool AWeaponBase::TryPickup(APawn* InPawn)
{
	if (!HasAuthority() || !InPawn || GetOwner() == InPawn)
	{
		return false;
	}

	USkeletalMeshComponent* PawnMesh = FindAttachMeshOnPawn(InPawn);

	if (!PawnMesh)
	{
		return false;
	}

	// 在设置owner前，先清理可能存在的旧widget
	DestroyFireWidget();

	SetOwner(InPawn);
	AttachToPawn(InPawn, PawnMesh);
	AssignWeaponToPawn(InPawn);

	// 玩家捡起武器后，创建 widget 和准星
	StartFireWidgetRetry();
	EnsureCrosshairWidget();

	if (PickupSphere)
	{
		PickupSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	return true;
}

USkeletalMeshComponent* AWeaponBase::FindAttachMeshOnPawn(APawn* InPawn) const
{
	if (!InPawn)
	{
		return nullptr;
	}

	TArray<USkeletalMeshComponent*> Meshes;
	InPawn->GetComponents<USkeletalMeshComponent>(Meshes);

	if (!GrabPointName.IsNone())
	{
		for (USkeletalMeshComponent* Mesh : Meshes)
		{
			if (!Mesh)
			{
				continue;
			}

			if (Mesh->DoesSocketExist(GrabPointName) || Mesh->GetBoneIndex(GrabPointName) != INDEX_NONE)
			{
				return Mesh;
			}
		}
	}

	if (ACharacter* Character = Cast<ACharacter>(InPawn))
	{
		if (USkeletalMeshComponent* CharacterMesh = Character->GetMesh())
		{
			return CharacterMesh;
		}
	}

	return Meshes.Num() > 0 ? Meshes[0] : nullptr;
}

void AWeaponBase::AttachToPawn(APawn* InPawn, USkeletalMeshComponent* PawnMesh)
{
	if (!PawnMesh)
	{
		return;
	}

	// 避免父级非等比缩放导致武器变形
	WeaponMesh->SetUsingAbsoluteScale(true);
	WeaponMesh->SetRelativeScale3D(FVector::OneVector);

	FTransform WeaponGripLocal;
	FTransform PawnGrabComponent;
	const bool bHasPawnGrab = GetMeshSocketOrBoneTransform(PawnMesh, GrabPointName, RTS_Component, PawnGrabComponent);
	const bool bHasWeaponGrip = GetMeshSocketOrBoneTransform(WeaponMesh, WeaponGripPointName, RTS_Component, WeaponGripLocal);

	if (bHasPawnGrab && bHasWeaponGrip)
	{
		const FAttachmentTransformRules AttachRules(
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepRelative,
			EAttachmentRule::KeepWorld,
			true);
		AttachToComponent(PawnMesh, AttachRules, GrabPointName);

		const FTransform DesiredRelative = WeaponGripLocal.Inverse();
		SetActorRelativeTransform(DesiredRelative, false, nullptr, ETeleportType::TeleportPhysics);
		HipTransform = DesiredRelative;
	}
	else
	{
		const FAttachmentTransformRules AttachRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld,
			true);
		const FName SocketName = GrabPointName.IsNone() ? NAME_None : GrabPointName;
		AttachToComponent(PawnMesh, AttachRules, SocketName);
		if (USceneComponent* Root = GetRootComponent())
		{
			HipTransform = Root->GetRelativeTransform();
		}
	}
	ADSAlpha = 0.0f;
	bIsADS = false;
	bHipTransformCaptured = true; // 服务器在本路径已设置 HipTransform
}

void AWeaponBase::AssignWeaponToPawn(APawn* InPawn)
{
	if (!InPawn || WeaponVariableName.IsNone())
	{
		return;
	}

	SetWeaponVariableOnPawn(InPawn);

	if (HasAuthority())
	{
		MulticastAssignWeaponToPawn(InPawn);
	}
}

void AWeaponBase::SetWeaponVariableOnPawn(APawn* InPawn)
{
	if (!InPawn || WeaponVariableName.IsNone())
	{
		return;
	}

	FProperty* Prop = InPawn->GetClass()->FindPropertyByName(WeaponVariableName);
	FObjectPropertyBase* ObjProp = CastField<FObjectPropertyBase>(Prop);
	if (!ObjProp)
	{
		return;
	}

	if (!ObjProp->PropertyClass->IsChildOf(AActor::StaticClass()))
	{
		return;
	}

	ObjProp->SetObjectPropertyValue_InContainer(InPawn, this);
}

void AWeaponBase::MulticastAssignWeaponToPawn_Implementation(APawn* InPawn)
{
	SetWeaponVariableOnPawn(InPawn);
}

FVector AWeaponBase::GetMuzzleLocation() const
{
	FVector MuzzleLoc;
	if (WeaponMesh && !FireSocketName.IsNone())
	{
		MuzzleLoc = WeaponMesh->GetSocketLocation(FireSocketName);
	}
	else
	{
		MuzzleLoc = WeaponMesh ? WeaponMesh->GetComponentLocation() : GetActorLocation();
	}

	// 开镜时应用可调偏移（武器本地空间：X 前 Y 右 Z 上），本地端与服务器均使用，便于手动调整开镜后枪口位置
	if (bIsADS && WeaponMesh && !ADSMuzzleOffset.IsNearlyZero())
	{
		MuzzleLoc += WeaponMesh->GetComponentQuat().RotateVector(ADSMuzzleOffset);
	}
	return MuzzleLoc;
}

FRotator AWeaponBase::GetAimRotation() const
{
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (AController* Controller = OwnerPawn->GetController())
		{
			return Controller->GetControlRotation();
		}
	}

	return WeaponMesh ? WeaponMesh->GetComponentRotation() : GetActorRotation();
}

void AWeaponBase::HandleLineTrace(const FVector& Origin, const FVector& Dir)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector TraceDir = Dir.GetSafeNormal();
	const FVector End = Origin + (TraceDir * TraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WeaponTrace), true);
	Params.AddIgnoredActor(this);
	if (AActor* OwnerActor = GetOwner())
	{
		Params.AddIgnoredActor(OwnerActor);
	}

	FHitResult HitResult;
	const bool bHit = World->LineTraceSingleByChannel(HitResult, Origin, End, ECC_Visibility, Params);
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

void AWeaponBase::HandleAutoFireTick()
{
	if (bIsReloading)
	{
		return;
	}

	Fire();
}

bool AWeaponBase::CanFire() const
{
	return !bIsReloading && CurrentAmmo > 0;
}

bool AWeaponBase::HasAmmo() const
{
	return CurrentAmmo > 0;
}

float AWeaponBase::GetFireInterval() const
{
	return FireRate > 0.0f ? (1.0f / FireRate) : 0.0f;
}

void AWeaponBase::ConsumeAmmo()
{
	if (CurrentAmmo <= 0)
	{
		return;
	}

	--CurrentAmmo;
	UpdateAmmoUI();

	if (bAutoReload && CurrentAmmo <= 0)
	{
		StartReloadInternal(true);
	}
}

void AWeaponBase::StartReloadInternal(bool bTriggeredByAuto)
{
	if (bIsReloading || MaxAmmo <= 0 || ReloadTime <= 0.0f)
	{
		return;
	}

	bReloadTriggeredByAuto = bTriggeredByAuto;
	bIsReloading = true;
	ReloadEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() + ReloadTime : 0.0f;
	StartReloadUI();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, ReloadTime, false);
	}
}

void AWeaponBase::FinishReload()
{
	CurrentAmmo = MaxAmmo;
	bIsReloading = false;
	ReloadEndTime = 0.0f;
	StopReloadUI();
	UpdateAmmoUI();

	// 仅当本次填弹是玩家主动按键触发时才在填弹完成后恢复开火；自动填弹（打空弹匣）后不自动开火
	if (bWantsToFire && !bReloadTriggeredByAuto)
	{
		StartFire();
	}
}

void AWeaponBase::StartReloadUI()
{
	UpdateReloadUI();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadUITimerHandle);
		World->GetTimerManager().SetTimer(ReloadUITimerHandle, this, &AWeaponBase::UpdateReloadUI, 0.1f, true);
	}
}

void AWeaponBase::StopReloadUI()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadUITimerHandle);
	}
	UpdateReloadUI();
}

void AWeaponBase::UpdateAmmoUI()
{
	if (!FireWidget)
	{
		return;
	}

	if (UWidgetTree* WidgetTree = FireWidget->WidgetTree)
	{
		if (UTextBlock* AmmoText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("CurrentBulletNumber"))))
		{
			AmmoText->SetText(FText::AsNumber(CurrentAmmo));
		}
	}
}

void AWeaponBase::UpdateReloadUI()
{
	if (!FireWidget)
	{
		return;
	}

	FText Text = FText::GetEmpty();
	if (bIsReloading && GetWorld())
	{
		const float Remaining = FMath::Max(0.0f, ReloadEndTime - GetWorld()->GetTimeSeconds());
		Text = FText::AsNumber(FMath::CeilToInt(Remaining));
	}

	if (UWidgetTree* WidgetTree = FireWidget->WidgetTree)
	{
		if (UTextBlock* CDText = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("FireCDTimerText"))))
		{
			CDText->SetText(Text);
		}
	}
}

void AWeaponBase::EnsureFireWidget()
{
	if (FireWidget || !FireWidgetClass)
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return;
	}

	FireWidget = CreateWidget<UUserWidget>(PC, FireWidgetClass);
	if (FireWidget)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FireWidgetRetryHandle);
		}
		FireWidget->AddToViewport();
		UpdateAmmoUI();
		UpdateReloadUI();
	}

	EnsureCrosshairWidget();
}

void AWeaponBase::DestroyFireWidget()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireWidgetRetryHandle);
	}

	if (FireWidget)
	{
		FireWidget->RemoveFromParent();
		FireWidget = nullptr;
	}

	DestroyCrosshairWidget();
}

void AWeaponBase::EnsureCrosshairWidget()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return;
	}

	auto CreateAndAdd = [PC, this](TSubclassOf<UUserWidget> Class, UUserWidget*& OutWidget)
	{
		if (!Class || OutWidget)
		{
			return;
		}
		OutWidget = CreateWidget<UUserWidget>(PC, Class);
		if (OutWidget)
		{
			OutWidget->AddToViewport();
		}
	};

	CreateAndAdd(CrosshairDefaultClass, CrosshairDefaultWidget);
	CreateAndAdd(CrosshairHitClass, CrosshairHitWidget);
	CreateAndAdd(CrosshairHeadshotClass, CrosshairHeadshotWidget);

	// 至少有一个准星时才设为默认状态
	if (CrosshairDefaultWidget || CrosshairHitWidget || CrosshairHeadshotWidget)
	{
		SetCrosshairStateInternal(ECrosshairState::Default);
	}
}

void AWeaponBase::DestroyCrosshairWidget()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CrosshairResetTimerHandle);
	}

	auto Remove = [](UUserWidget*& W)
	{
		if (W)
		{
			W->RemoveFromParent();
			W = nullptr;
		}
	};
	Remove(CrosshairDefaultWidget);
	Remove(CrosshairHitWidget);
	Remove(CrosshairHeadshotWidget);
}

void AWeaponBase::CleanupFireWidget()
{
	DestroyFireWidget();
	
	// 如果是服务器，通知所有客户端也清理widget
	if (HasAuthority())
	{
		ClientCleanupFireWidget();
	}
}

void AWeaponBase::ClientCleanupFireWidget_Implementation()
{
	DestroyFireWidget();
}

void AWeaponBase::StartFireWidgetRetry()
{
	if (FireWidget || !FireWidgetClass)
	{
		return;
	}

	EnsureFireWidget();
	EnsureCrosshairWidget();

	if (FireWidget)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireWidgetRetryHandle);
		World->GetTimerManager().SetTimer(FireWidgetRetryHandle, this, &AWeaponBase::EnsureFireWidget, 0.2f, true);
	}
}

void AWeaponBase::OnRep_CurrentAmmo()
{
	UpdateAmmoUI();
}

void AWeaponBase::OnRep_Reloading()
{
	if (bIsReloading)
	{
		ReloadEndTime = GetWorld() ? GetWorld()->GetTimeSeconds() + ReloadTime : 0.0f;
		StartReloadUI();
	}
	else
	{
		StopReloadUI();
	}
}

void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponBase, CurrentAmmo);
	DOREPLIFETIME(AWeaponBase, bIsReloading);
	DOREPLIFETIME(AWeaponBase, bIsADS);
}


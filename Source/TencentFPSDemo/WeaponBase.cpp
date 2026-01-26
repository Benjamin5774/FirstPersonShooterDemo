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
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "UObject/UnrealType.h"

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
	PrimaryActorTick.bCanEverTick = false;
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
	bAutoReload = true;
	ReloadEndTime = 0.0f;
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
	}
	else
	{
		// 确保没有残留的widget
		DestroyFireWidget();
	}
}

void AWeaponBase::OnRep_Owner()
{
	Super::OnRep_Owner();

	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		SetWeaponVariableOnPawn(OwnerPawn);
		StartFireWidgetRetry();
	}
	else
	{
		DestroyFireWidget();
	}
}

void AWeaponBase::Fire()
{
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

	if (OwnerPawn->IsLocallyControlled() && FireSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation(), FireSoundVolume);
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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FireTimerHandle);
	}
}

void AWeaponBase::StartReload()
{
	if (HasAuthority())
	{
		StartReloadInternal();
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
			StartReloadInternal();
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
	StartReloadInternal();
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

	// 玩家捡起武器后，创建widget
	StartFireWidgetRetry();

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
	}
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
	if (WeaponMesh && !FireSocketName.IsNone())
	{
		return WeaponMesh->GetSocketLocation(FireSocketName);
	}

	return WeaponMesh ? WeaponMesh->GetComponentLocation() : GetActorLocation();
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
		StartReloadInternal();
	}
}

void AWeaponBase::StartReloadInternal()
{
	if (bIsReloading || MaxAmmo <= 0 || ReloadTime <= 0.0f)
	{
		return;
	}

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

	if (bWantsToFire)
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
}

void AWeaponBase::CleanupFireWidget()
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
}


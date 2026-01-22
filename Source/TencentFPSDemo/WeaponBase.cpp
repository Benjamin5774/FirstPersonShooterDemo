#include "WeaponBase.h"
#include "HealthComponent.h"
#include "FPSPlayerState.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
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
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (PickupSphere)
	{
		PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &AWeaponBase::OnPickupSphereBeginOverlap);
	}
}

void AWeaponBase::Fire()
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
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

	SetOwner(InPawn);
	AttachToPawn(InPawn, PawnMesh);
	AssignWeaponToPawn(InPawn);

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

	const FVector MuzzleLocation = GetMuzzleLocation();
	const FRotator AimRotation = GetAimRotation();
	const FVector AimDir = AimRotation.Vector().GetSafeNormal();

	if (BulletClass)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = OwnerPawn;
		SpawnParams.Instigator = OwnerPawn;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		GetWorld()->SpawnActor<AActor>(BulletClass, MuzzleLocation, AimRotation, SpawnParams);
	}

	if (!AimDir.IsNearlyZero())
	{
		HandleLineTrace(MuzzleLocation, AimDir);
	}
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

	SetOwner(InPawn);
	AttachToPawn(InPawn, PawnMesh);
	AssignWeaponToPawn(InPawn);

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


#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UHealthComponent;
class APawn;
class UUserWidget;
class UTextBlock;
class USoundBase;

//准星显示, 默认 / 命中身体 / 爆头
UENUM(BlueprintType)
enum class ECrosshairState : uint8
{
	Default   UMETA(DisplayName = "默认（未命中）"),
	Hit       UMETA(DisplayName = "命中身体"),
	Headshot  UMETA(DisplayName = "爆头")
};

UCLASS()
class TENCENTFPSDEMO_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void StartReload();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void SetFireWidget(UUserWidget* InWidget);

	//设置准星状态
	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void SetCrosshairState(ECrosshairState State);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	static AWeaponBase* GetWeaponFromPawn(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipToPawn(APawn* InPawn);

	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon|Ammo")
	bool HasAmmo() const;

	UFUNCTION(BlueprintCallable, Category = "Weapon|ADS")
	void SetADS(bool bAiming);

	//当前是否处于开镜状态
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon|ADS")
	bool IsADS() const { return bIsADS; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;
	virtual void OnRep_Owner() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(Server, Reliable)
	void ServerFire();

	UFUNCTION(Server, Reliable)
	void ServerStartReload();

	UFUNCTION()
	void OnPickupSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	void HandleLineTrace(const FVector& Origin, const FVector& Dir);
	FVector GetMuzzleLocation() const;
	FRotator GetAimRotation() const;
	bool TryPickup(APawn* InPawn);
	USkeletalMeshComponent* FindAttachMeshOnPawn(APawn* InPawn) const;
	void AttachToPawn(APawn* InPawn, USkeletalMeshComponent* PawnMesh);
	void AssignWeaponToPawn(APawn* InPawn);
	void SetWeaponVariableOnPawn(APawn* InPawn);

	void HandleAutoFireTick();
	bool CanFire() const;
	float GetFireInterval() const;
	void ConsumeAmmo();
	void FinishReload();

	void StartReloadInternal(bool bTriggeredByAuto = false);
	void StartReloadUI();
	void StopReloadUI();
	void UpdateAmmoUI();
	void UpdateReloadUI();
	void EnsureFireWidget();
	void DestroyFireWidget();
	void StartFireWidgetRetry();
	void EnsureCrosshairWidget();
	void DestroyCrosshairWidget();
	void SetCrosshairStateInternal(ECrosshairState State);
	void ResetCrosshairToDefault();

	void ApplyRecoil();

	void UpdateADSTransform(float DeltaTime);

public:
	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void CleanupFireWidget();

	UFUNCTION()
	void OnRep_CurrentAmmo();

	UFUNCTION()
	void OnRep_Reloading();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAssignWeaponToPawn(APawn* InPawn);

	UFUNCTION(Client, Reliable)
	void ClientCleanupFireWidget();

	UFUNCTION(Client, Reliable)
	void ClientSetCrosshairState(ECrosshairState State);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	USkeletalMeshComponent* WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon", meta = (AllowPrivateAccess = "true"))
	USphereComponent* PickupSphere;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TSubclassOf<AActor> BulletClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName FireSocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName GrabPointName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName WeaponGripPointName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	FName WeaponVariableName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Audio")
	USoundBase* FireSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Audio")
	float FireSoundVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float TraceDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Damage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Fire")
	float FireRate;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	bool bIsFiring = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RecoilPitch = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RecoilYaw = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	int32 MaxAmmo;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentAmmo, VisibleInstanceOnly, Category = "Weapon|Ammo")
	int32 CurrentAmmo;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	float ReloadTime;

	UPROPERTY(ReplicatedUsing = OnRep_Reloading, VisibleInstanceOnly, Category = "Weapon|Ammo")
	bool bIsReloading;

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|Ammo")
	bool bWantsToFire;

	bool bReloadTriggeredByAuto = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	bool bAutoReload;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> FireWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair")
	TSubclassOf<UUserWidget> CrosshairDefaultClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair")
	TSubclassOf<UUserWidget> CrosshairHitClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair")
	TSubclassOf<UUserWidget> CrosshairHeadshotClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float CrosshairResetTime;

	// ADS插值枪 Mesh 
	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|ADS", meta = (AllowPrivateAccess = "true"))
	FTransform HipTransform;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|ADS")
	FTransform ADSRelativeOffset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|ADS")
	FVector ADSMuzzleOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|ADS", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ADSSpeed = 10.0f;

	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Weapon|ADS", meta = (AllowPrivateAccess = "true"))
	bool bIsADS = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|ADS", meta = (AllowPrivateAccess = "true"))
	float ADSAlpha = 0.0f;

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|ADS", meta = (AllowPrivateAccess = "true"))
	bool bHipTransformCaptured = false;

	UPROPERTY(Transient)
	UUserWidget* FireWidget;

	UPROPERTY(Transient)
	UUserWidget* CrosshairDefaultWidget;

	UPROPERTY(Transient)
	UUserWidget* CrosshairHitWidget;

	UPROPERTY(Transient)
	UUserWidget* CrosshairHeadshotWidget;

	FTimerHandle CrosshairResetTimerHandle;

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|Ammo")
	float ReloadEndTime;

	FTimerHandle FireTimerHandle;
	FTimerHandle ReloadTimerHandle;
	FTimerHandle ReloadUITimerHandle;
	FTimerHandle FireWidgetRetryHandle;
};


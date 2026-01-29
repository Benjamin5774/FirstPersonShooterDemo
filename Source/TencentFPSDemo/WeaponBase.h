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

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipToPawn(APawn* InPawn);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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
	void StartReloadInternal();
	void StartReloadUI();
	void StopReloadUI();
	void UpdateAmmoUI();
	void UpdateReloadUI();
	void EnsureFireWidget();
	void DestroyFireWidget();
	void StartFireWidgetRetry();

	/** 仅本地控制的玩家开火时调用：视角后坐力（相机震动由蓝图实现） */
	void ApplyRecoil();

public:
	// 清理widget的公共接口，用于游戏重新开始时清理所有武器的widget
	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void CleanupFireWidget();

	UFUNCTION()
	void OnRep_CurrentAmmo();

	UFUNCTION()
	void OnRep_Reloading();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAssignWeaponToPawn(APawn* InPawn);

	// 客户端清理widget的RPC（用于确保客户端在死亡时也能清理widget）
	UFUNCTION(Client, Reliable)
	void ClientCleanupFireWidget();

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

	/** 每发子弹视角上抬角度（度），典型 0.3~1.0 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Recoil", meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RecoilPitch = 0.5f;

	/** 每发子弹视角水平偏移角度（度），可加随机；0 表示仅垂直后坐力 */
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	bool bAutoReload;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> FireWidgetClass;

	UPROPERTY(Transient)
	UUserWidget* FireWidget;

	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|Ammo")
	float ReloadEndTime;

	FTimerHandle FireTimerHandle;
	FTimerHandle ReloadTimerHandle;
	FTimerHandle ReloadUITimerHandle;
	FTimerHandle FireWidgetRetryHandle;
};


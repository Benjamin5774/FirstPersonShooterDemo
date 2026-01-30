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

/** 准星显示状态：默认 / 命中身体 / 爆头 */
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

	/** 设置准星状态（默认 / 命中 / 爆头），蓝图可调；命中/爆头会在短暂时间后自动恢复为默认 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|UI")
	void SetCrosshairState(ECrosshairState State);

	/** 从 Pawn 上取得当前装备的武器（用于子弹命中时通知准星） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon")
	static AWeaponBase* GetWeaponFromPawn(APawn* Pawn);

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	bool EquipToPawn(APawn* InPawn);

	/** 检测当前是否有子弹（CurrentAmmo > 0） */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Weapon|Ammo")
	bool HasAmmo() const;

	/** 开镜（瞄准）：true 进入 ADS，false 回到腰射；按住式开镜由输入在 Pressed/Released 时调用 */
	UFUNCTION(BlueprintCallable, Category = "Weapon|ADS")
	void SetADS(bool bAiming);

	/** 当前是否处于开镜状态（蓝图可读） */
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
	/** bTriggeredByAuto: 是否为打空弹匣触发的自动填弹（为 true 时填弹完成后不会自动恢复开火） */
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

	/** 仅本地控制的玩家开火时调用：视角后坐力（相机震动由蓝图实现） */
	void ApplyRecoil();

	/** 方案 A：Tick 内根据 ADSAlpha 插值 HipTransform <-> ADSTransform，设置 WeaponMesh 所在 Actor 的 RelativeTransform */
	void UpdateADSTransform(float DeltaTime);

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

	/** 服务器通知客户端：子弹命中，切换准星为命中/爆头（仅射击者客户端执行） */
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

	/** 当前帧是否正在打出子弹（有子弹且成功开火时为 true，否则为 false；蓝图可读） */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Weapon|Fire")
	bool bIsFiring = false;

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

	/** 当前这次填弹是否由自动填弹触发（打空弹匣）；填弹完成后若为 true 则不自动恢复开火 */
	bool bReloadTriggeredByAuto = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Ammo")
	bool bAutoReload;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI")
	TSubclassOf<UUserWidget> FireWidgetClass;

	/** 默认准星（未命中时显示） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair")
	TSubclassOf<UUserWidget> CrosshairDefaultClass;

	/** 命中身体时显示的准星 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair")
	TSubclassOf<UUserWidget> CrosshairHitClass;

	/** 爆头时显示的准星 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair")
	TSubclassOf<UUserWidget> CrosshairHeadshotClass;

	/** 命中/爆头准星显示多久后恢复为默认（秒） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|UI|Crosshair", meta = (ClampMin = "0.05", UIMin = "0.05"))
	float CrosshairResetTime;

	// ---------- 开镜（ADS）方案 A：插值枪 Mesh ----------
	/** 腰射时武器相对附着点的 Transform（装备时从 Attach 结果写入，用于插值起点） */
	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|ADS", meta = (AllowPrivateAccess = "true"))
	FTransform HipTransform;

	/** 开镜时相对腰射的偏移（与 HipTransform 相乘得到 ADSTransform；可在蓝图/编辑器中调“枪靠近眼睛”） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|ADS")
	FTransform ADSRelativeOffset;

	/** 开镜时枪口位置偏移（武器本地空间：X 前 / Y 右 / Z 上），用于 GetMuzzleLocation() 在开镜后的修正，可手动调整本地端开镜后的枪口位置 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|ADS")
	FVector ADSMuzzleOffset = FVector::ZeroVector;

	/** 开镜插值速度（Alpha 向 0/1 靠近的速率） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|ADS", meta = (ClampMin = "0.1", UIMin = "0.1"))
	float ADSSpeed = 10.0f;

	/** 当前是否请求开镜（由输入 SetADS 设置）；复制到服务器以便 GetMuzzleLocation 在开镜时应用 ADSMuzzleOffset */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Weapon|ADS", meta = (AllowPrivateAccess = "true"))
	bool bIsADS = false;

	/** 开镜插值系数 0=腰射 1=开镜，Tick 中插值 */
	UPROPERTY(VisibleInstanceOnly, Category = "Weapon|ADS", meta = (AllowPrivateAccess = "true"))
	float ADSAlpha = 0.0f;

	/** 是否已写入 HipTransform（服务器在 AttachToPawn 写入；客户端首次 Tick 从当前相对变换捕获） */
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


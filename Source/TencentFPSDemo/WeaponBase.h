#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WeaponBase.generated.h"

class USkeletalMeshComponent;
class USphereComponent;
class UHealthComponent;

UCLASS()
class TENCENTFPSDEMO_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void Fire();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(Server, Reliable)
	void ServerFire();

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

	UFUNCTION(NetMulticast, Reliable)
	void MulticastAssignWeaponToPawn(APawn* InPawn);

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
	FName WeaponVariableName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float TraceDistance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	float Damage;
};


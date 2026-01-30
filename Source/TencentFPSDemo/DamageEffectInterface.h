#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageEffectInterface.generated.h"

UINTERFACE(BlueprintType)
class UDamageEffectInterface : public UInterface
{
	GENERATED_BODY()
};

//Interface for damage popup actors (DamageNumberActor etc.); init before spawn/replicate.
//伤害飘字 Actor 接口；在生成/复制前初始化。
class TENCENTFPSDEMO_API IDamageEffectInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DamageEffect")
	void InitDamageEffect(float Damage, FLinearColor Color);
};


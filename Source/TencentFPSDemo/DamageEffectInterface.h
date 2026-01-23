#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DamageEffectInterface.generated.h"

UINTERFACE(BlueprintType)
class UDamageEffectInterface : public UInterface
{
	GENERATED_BODY()
};

class TENCENTFPSDEMO_API IDamageEffectInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "DamageEffect")
	void InitDamageEffect(float Damage, FLinearColor Color);
};


#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapMomentCallback.generated.h"
UINTERFACE(BlueprintType) class TAPMOMENT_API UTapMomentCallback:public UInterface{GENERATED_BODY()};
class TAPMOMENT_API ITapMomentCallback{GENERATED_BODY() public:UFUNCTION(BlueprintNativeEvent,Category="TapMoment")void OnMomentResult(int32 Code,const FString& Message);};

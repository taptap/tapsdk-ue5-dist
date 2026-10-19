#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapCloudSaveCallback.generated.h"
UINTERFACE(BlueprintType) class TAPCLOUDSAVE_API UTapCloudSaveCallback:public UInterface{GENERATED_BODY()};
class TAPCLOUDSAVE_API ITapCloudSaveCallback{GENERATED_BODY() public:UFUNCTION(BlueprintNativeEvent,Category="TapCloudSave")void OnResult(int32 ResultCode);};

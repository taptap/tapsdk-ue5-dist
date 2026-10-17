#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapLicenseTypes.h"
#include "TapLicenseCallback.generated.h"
UINTERFACE(BlueprintType) class TAPLICENSE_API UTapLicenseCallback:public UInterface{GENERATED_BODY()};
class TAPLICENSE_API ITapLicenseCallback{GENERATED_BODY() public:UFUNCTION(BlueprintNativeEvent,Category="TapLicense")void OnLicenseSuccess();UFUNCTION(BlueprintNativeEvent,Category="TapLicense")void OnLicenseFailed();};
UINTERFACE(BlueprintType) class TAPLICENSE_API UTapDLCCallback:public UInterface{GENERATED_BODY()};
class TAPLICENSE_API ITapDLCCallback{GENERATED_BODY() public:UFUNCTION(BlueprintNativeEvent,Category="TapLicense")void OnQueryCallback(ETapLicenseQueryCode Code,const TMap<FString,bool>& Results);UFUNCTION(BlueprintNativeEvent,Category="TapLicense")void OnOrderCallback(const FString& Sku,ETapDLCPurchasedCode Status);};

#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapLeaderboardCallback.generated.h"
UINTERFACE(BlueprintType) class TAPLEADERBOARD_API UTapLeaderboardCallback:public UInterface{GENERATED_BODY()};
class TAPLEADERBOARD_API ITapLeaderboardCallback{GENERATED_BODY() public:UFUNCTION(BlueprintNativeEvent,Category="TapLeaderboard")void OnLeaderboardResult(int32 Code,const FString& Message);};
UINTERFACE(BlueprintType) class TAPLEADERBOARD_API UTapLeaderboardShareCallback:public UInterface{GENERATED_BODY()};
class TAPLEADERBOARD_API ITapLeaderboardShareCallback{GENERATED_BODY() public:UFUNCTION(BlueprintNativeEvent,Category="TapLeaderboard")void OnShareSuccess(const FString& LocalPath);UFUNCTION(BlueprintNativeEvent,Category="TapLeaderboard")void OnShareFailed(const FString& ErrorMessage);};

#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapAchievementTypes.h"
#include "TapAchievementCallback.generated.h"
UINTERFACE(BlueprintType) class TAPACHIEVEMENT_API UTapAchievementCallback:public UInterface{GENERATED_BODY()};
class TAPACHIEVEMENT_API ITapAchievementCallback{GENERATED_BODY() public:
 UFUNCTION(BlueprintNativeEvent,Category="TapAchievement") void OnAchievementSuccess(int32 Code,const FTapAchievementResult& Result);
 UFUNCTION(BlueprintNativeEvent,Category="TapAchievement") void OnAchievementFailure(const FString& AchievementId,int32 ErrorCode,const FString& ErrorMessage);
};

// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapMomentTypes.h"
#include "TapMomentCallback.h"
#include "TapMoment.generated.h"

UCLASS()
class TAPMOMENT_API UTapMoment : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="TapMoment")
    static void Open();

    UFUNCTION(BlueprintCallable, Category="TapMoment")
    static void OpenScene(const FString& SceneId);

    UFUNCTION(BlueprintCallable, Category="TapMoment")
    static void Publish(const FTapMomentPostData& PostData);

    UFUNCTION(BlueprintCallable, Category="TapMoment")
    static void Close();

    UFUNCTION(BlueprintCallable, Category="TapMoment")
    static void CloseWithTitle(const FString& Title, const FString& Content);

    UFUNCTION(BlueprintCallable, Category="TapMoment")
    static void FetchNotification();

    /** 注册持久回调（调用一次）；事件通过 OnMomentCallback 广播 */
    UFUNCTION(BlueprintCallable, Category="TapMoment")
    static void SetCallback(TScriptInterface<ITapMomentCallback> Callback);
};

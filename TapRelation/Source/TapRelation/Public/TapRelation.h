// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapRelationCallback.h"
#include "TapRelation.generated.h"

UCLASS()
class TAPRELATION_API UTapRelation : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void Prepare();
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void StartMessenger();
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void InviteGame();
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void InviteTeam(const FString& TeamId);
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void ShowTapUserProfile(const FString& OpenId, const FString& UnionId);
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void RegisterRelationCallback(TScriptInterface<ITapRelationCallback> Callback);
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void UnregisterRelationCallback(TScriptInterface<ITapRelationCallback> Callback);
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void RegisterRelationInviteCallback(TScriptInterface<ITapRelationInviteCallback> Callback);
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void UnregisterRelationInviteCallback(TScriptInterface<ITapRelationInviteCallback> Callback);
    UFUNCTION(BlueprintCallable, Category="TapRelation") static void Destroy();

    // per-call（C++ only，TFunction 不支持 Blueprint 直接调用）
    static void GetNewFansCount(TFunction<void(int32)> Callback);
    static void GetUnreadMessageCount(TFunction<void(int32)> Callback);
};

// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapRelationLiteTypes.h"
#include "TapRelationLiteCallback.h"
#include "TapRelationLite.generated.h"

UCLASS()
class TAPRELATIONLITE_API UTapRelationLite : public UObject
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="TapRelationLite") static void InviteGame();
    UFUNCTION(BlueprintCallable, Category="TapRelationLite") static void InviteTeam(const FString& TeamId);
    UFUNCTION(BlueprintCallable, Category="TapRelationLite") static void ShowTapUserProfile(const FString& OpenId, const FString& UnionId);
    UFUNCTION(BlueprintCallable, Category="TapRelationLite") static void RegisterRelationLiteCallback(TScriptInterface<ITapRelationLiteCallback> Callback);
    UFUNCTION(BlueprintCallable, Category="TapRelationLite") static void UnregisterRelationLiteCallback(TScriptInterface<ITapRelationLiteCallback> Callback);
    UFUNCTION(BlueprintCallable, Category="TapRelationLite") static void RegisterRelationLiteInviteCallback(TScriptInterface<ITapRelationLiteInviteCallback> Callback);
    UFUNCTION(BlueprintCallable, Category="TapRelationLite") static void UnregisterRelationLiteInviteCallback(TScriptInterface<ITapRelationLiteInviteCallback> Callback);

    static void GetFriendsList(const FString& NextPageToken,
        TFunction<void(const FTapRelationLiteUserResult&)> OnSuccess,
        TFunction<void(int32, const FString&)> OnFailure);
    static void GetFollowingList(const FString& NextPageToken,
        TFunction<void(const FTapRelationLiteUserResult&)> OnSuccess,
        TFunction<void(int32, const FString&)> OnFailure);
    static void GetFansList(const FString& NextPageToken,
        TFunction<void(const FTapRelationLiteUserResult&)> OnSuccess,
        TFunction<void(int32, const FString&)> OnFailure);
    static void SyncRelationshipWithOpenId(int32 Action, const FString& Nickname,
        const FString& FriendNickname, const FString& FriendOpenId,
        TFunction<void()> OnSuccess, TFunction<void(const FString&)> OnFailure);
    static void SyncRelationshipWithUnionId(int32 Action, const FString& Nickname,
        const FString& FriendNickname, const FString& FriendUnionId,
        TFunction<void()> OnSuccess, TFunction<void(const FString&)> OnFailure);
};

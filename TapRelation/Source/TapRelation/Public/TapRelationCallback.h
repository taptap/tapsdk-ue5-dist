// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapRelationCallback.generated.h"

UINTERFACE(BlueprintType)
class TAPRELATION_API UTapRelationCallback : public UInterface { GENERATED_BODY() };
class TAPRELATION_API ITapRelationCallback
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category="TapRelation") void OnMessengerCodeResult(int32 Code);
	UFUNCTION(BlueprintNativeEvent, Category="TapRelation") void OnNewFansCountChanged(int32 Code, int32 NewFansCount);
	UFUNCTION(BlueprintNativeEvent, Category="TapRelation") void OnUnreadMessageCountChanged(int32 Code, int32 UnreadMessageCount);
};

UINTERFACE(BlueprintType)
class TAPRELATION_API UTapRelationInviteCallback : public UInterface { GENERATED_BODY() };
class TAPRELATION_API ITapRelationInviteCallback
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintNativeEvent, Category="TapRelation") void OnGameInviteReceived(const FString& OpenId, const FString& UnionId);
	UFUNCTION(BlueprintNativeEvent, Category="TapRelation") void OnTeamInviteReceived(const FString& OpenId, const FString& UnionId, const FString& TeamId);
};

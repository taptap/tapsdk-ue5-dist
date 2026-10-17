// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapRelationLiteTypes.generated.h"

// 持久回调委托，relation_lite_result_code JSON 值是字符串 "N"，已在桥接层转为 int32
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRelationLiteCallback, int32 /*Code*/);

USTRUCT(BlueprintType)
struct TAPRELATIONLITE_API FTapFollowStatus
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") bool bBlockedBy  = false;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") bool bBlocking   = false;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") bool bFollowedBy = false;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") bool bFollowing  = false;
};

USTRUCT(BlueprintType)
struct TAPRELATIONLITE_API FTapRelationLiteUserInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") FString Alias;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") FString Avatar;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") FString Name;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") FString OpenId;
};

USTRUCT(BlueprintType)
struct TAPRELATIONLITE_API FTapRelationLiteUserItem
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") FTapRelationLiteUserInfo User;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") FTapFollowStatus FollowStatus;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") int64 CreatedAt = 0;
};

USTRUCT(BlueprintType)
struct TAPRELATIONLITE_API FTapRelationLiteUserResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") TArray<FTapRelationLiteUserItem> Users;
    UPROPERTY(BlueprintReadOnly, Category="TapRelationLite") FString NextPageToken;
};

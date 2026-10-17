// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapRelationLiteTypes.h"

namespace TapRelationLiteListResult
{
    struct FParsed
    {
        bool bSuccess = false;
        FTapRelationLiteUserResult Result;
        FString ErrorMessage = TEXT("Invalid RelationLite list response");
    };

    // iOS native bridge encodes the list as a JSON string inside the result object.
    FParsed Parse(const FString& Json, const FString& ListKey);
    void Complete(const FString& Json, const FString& ListKey,
        TFunction<void(const FTapRelationLiteUserResult&)> OnSuccess,
        TFunction<void(int32, const FString&)> OnFailure, TFunction<bool()> IsCurrent = {});
}

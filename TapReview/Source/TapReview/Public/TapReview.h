// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapReview.generated.h"

/**
 * TapReview 门面（Android only，其它平台 no-op）
 * 对齐 Unity TapTapReview.openReview()
 */
UCLASS()
class TAPREVIEW_API UTapReview : public UObject
{
    GENERATED_BODY()
public:
    /** 唤起评价界面（Android only，非 Android 为 no-op + Warning）*/
    UFUNCTION(BlueprintCallable, Category="TapReview")
    static void OpenReview();
};

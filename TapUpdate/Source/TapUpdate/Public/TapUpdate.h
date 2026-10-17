// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapUpdate.generated.h"

DECLARE_DYNAMIC_DELEGATE(FTapUpdateCancelCallback);

/**
 * TapUpdate 门面
 *
 * 仅 Android 平台有效；非 Android 编译为 no-op。
 * 对齐 Unity TapTapUpdate.UpdateGame / CheckForceUpdate。
 */
UCLASS()
class TAPUPDATE_API UTapUpdate : public UObject
{
    GENERATED_BODY()
public:
    /**
     * 唤起 TapTap 更新 UI。
     * 用户取消时广播 OnUpdateCancel。
     * Android only — 其它平台 no-op。
     */
    UFUNCTION(BlueprintCallable, Category="TapUpdate")
    static void UpdateGame(const FTapUpdateCancelCallback& OnCancel);

    /**
     * 触发强制更新检查（fire-and-forget，无回调）。
     * Android only — 其它平台 no-op。
     */
    UFUNCTION(BlueprintCallable, Category="TapUpdate")
    static void CheckForceUpdate();
};

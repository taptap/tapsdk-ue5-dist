// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "TapAchievementTypes.h"
#include "TapAchievementCallback.h"
#include "TapAchievement.generated.h"

class UUserWidget;

UCLASS()
class TAPACHIEVEMENT_API UTapAchievement : public UObject
{
    GENERATED_BODY()
public:
    UTapAchievement();

    UFUNCTION(BlueprintCallable,Category="TapAchievement") static void RegisterCallback(TScriptInterface<ITapAchievementCallback> Callback);
    UFUNCTION(BlueprintCallable,Category="TapAchievement") static void UnregisterCallback(TScriptInterface<ITapAchievementCallback> Callback);

    /** 解锁成就（对齐 Unity TapTapAchievement.Unlock） */
    UFUNCTION(BlueprintCallable, Category="TapAchievement")
    static void Unlock(const FString& AchievementId);

    /** 增加分步成就进度（对齐 Unity TapTapAchievement.Increment） */
    UFUNCTION(BlueprintCallable, Category="TapAchievement")
    static void Increment(const FString& AchievementId, int32 Step);

    /** 用浏览器打开成就列表页（对齐 Unity TapTapAchievement.ShowAchievements） */
    UFUNCTION(BlueprintCallable, Category="TapAchievement")
    static void ShowAchievements();

    /** 控制解锁时的 Toast 通知（对齐 Unity TapTapAchievement.SetToastEnable） */
    UFUNCTION(BlueprintCallable, Category="TapAchievement")
    static void SetToastEnable(bool bEnable);

private:
    /**
     * 对 WBP_TapAchievementToast 的 CDO 硬引用，确保插件 Content 被 cooker 静态分析发现并打进包。
     * 见 docs/ue5/knowledge-base/packaging-plugin-content.md 第 1 要素（缺失会导致打包后 Toast 加载失败）。
     */
    UPROPERTY()
    TSubclassOf<UUserWidget> ToastWidgetClass;
};

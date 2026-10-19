// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TapAchievementTypes.h"
#include "TapAchievementToastWidget.generated.h"

class UTextBlock;
class UImage;

/**
 * Achievement Toast Widget 基类
 *
 * WBP_TapAchievementToast 的父类。Blueprint 必须继承此类并实现
 * OnSetAchievementData 事件来渲染 Toast，动画结束后调用 NotifyAnimationFinished()。
 *
 * 对照 Unity TapAchievementToast.cs / TapAchievementToastAC.anim：
 *   - SetAchievementData → 设置 icon / congratulationText / achievementName 并播放动画
 *   - 动画总时长 3.5s，单阶段缩放（与 Unity 一致，恭喜文案 + 成就名 + 图标同时显示）：
 *       0.00~0.25s : 缩放展开
 *       0.25~3.25s : 停留 3s
 *       3.25~3.50s : 缩放收缩
 *   - 动画结束 → 调用 NotifyAnimationFinished() → C++ 侧移除 Widget 并显示下一条
 */
UCLASS(Abstract, Blueprintable)
class TAPACHIEVEMENT_API UTapAchievementToastWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * C++ 侧调用：将成就数据推送给 Blueprint 进行渲染并启动动画。
     * Blueprint 重写 OnSetAchievementData 来响应。
     */
    void SetAchievementData(const FTapAchievementResult& InResult);

    /**
     * Blueprint 动画结束后调用此函数，通知 C++ 侧关闭当前 Toast 并显示队列中的下一条。
     * 对照 Unity TapAchievementToast.OnPlayEnd()。
     */
    UFUNCTION(BlueprintCallable, Category = "TapAchievement|Toast")
    void NotifyAnimationFinished();

    /**
     * Toast 被点击时调用，打开成就列表页。
     * 对照 Unity TapAchievementToast.OnClick() → TapTapAchievement.ShowAchievements()
     * Blueprint 中将整个 Toast 背景板的 OnClicked 绑定到此函数即可。
     */
    UFUNCTION(BlueprintCallable, Category = "TapAchievement|Toast")
    void OnToastClicked();

protected:
    // === 控件绑定：WBP 中必须存在同名、同类型的控件（meta=BindWidget）===
    // 名称不一致或类型不符会导致 WBP 编译报错：
    //   "A required widget binding ... was not found"
    // 数据填充与动画播放统一在 C++ SetAchievementData 中完成（见 .cpp）。

    /** 成就名称文本（对齐 Unity title） */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> AchievementNameText;

    /** 恭喜文案文本（Normal/Platinum 两套，由 GetCongratulationText 填充） */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
    TObjectPtr<UTextBlock> CongratulationText;

    /** 成就图标（固定 T_Achievement_Icon，可在 Designer 直接设死，故 Optional） */
    UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
    TObjectPtr<UImage> IconImage;

    /**
     * 可选的 Blueprint 扩展钩子：C++ 完成标准数据绑定与动画播放后回调。
     * 默认无需实现；仅当 WBP 想追加额外表现（粒子 / 音效等）时重写。
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "TapAchievement|Toast")
    void OnSetAchievementData(const FTapAchievementResult& Result);

    /**
     * 根据成就类型返回恭喜文案（对照 Unity TapAchievementToast）。
     *   Normal   → "恭喜解锁新成就"
     *   Platinum → "恭喜解锁白金成就"
     */
    UFUNCTION(BlueprintPure, Category = "TapAchievement|Toast")
    static FText GetCongratulationText(const FTapAchievementResult& Result);

    /** 已显示的成就数据（Blueprint 可读） */
    UPROPERTY(BlueprintReadOnly, Category = "TapAchievement|Toast")
    FTapAchievementResult CurrentResult;

    // === 交互与动画全部由 C++ 驱动（无需 BP 连线 / WidgetAnimation 资产）===

    /** 点击 Toast → 打开成就列表（替代 BP 的 OnMouseButtonDown 绑定） */
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent) override;

    /** 每帧驱动程序化缩放 / 淡入淡出动画 */
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    /** 启动程序化 Toast 动画（对照 Unity animation.Play("TapAchievementToastAC")） */
    void StartToastAnimation();

    // 动画时间线（对照 Unity TapAchievementToastAC.anim，总时长 3.5s）
    static constexpr float ExpandDuration   = 0.25f;  // 缩放展开
    static constexpr float HoldDuration     = 3.00f;  // 停留
    static constexpr float CollapseDuration = 0.25f;  // 缩放收缩

    float AnimElapsed  = 0.f;
    bool  bAnimPlaying = false;
};

// Copyright TapTap. All Rights Reserved.
#include "TapAchievementToastWidget.h"
#include "TapAchievement.h"
#include "TapAchievementModule.h"
#include "Desktop/TapAchievementToastManager.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UTapAchievementToastWidget::SetAchievementData(const FTapAchievementResult& InResult)
{
    CurrentResult = InResult;

    // 1. 数据绑定（对照 Unity TapAchievementToast.InitContent）
    // 字体大小对齐 Unity prefab（m_FontSize: 25）
    static constexpr int32 ToastFontSize = 25;
    // 现有 456px 卡片扣除 Border、图标和 Slot 边距后，文本列可用宽度为 289px。
    static constexpr float AchievementNameWrapWidth = 289.f;

    if (AchievementNameText)
    {
        AchievementNameText->SetText(FText::FromString(InResult.AchievementName));
        AchievementNameText->SetClipping(EWidgetClipping::ClipToBoundsAlways);
        AchievementNameText->SetAutoWrapText(true);
        AchievementNameText->SetWrapTextAt(AchievementNameWrapWidth);
        AchievementNameText->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
        AchievementNameText->SetTextOverflowPolicy(ETextOverflowPolicy::Clip);
        FSlateFontInfo NameFont = AchievementNameText->GetFont();
        NameFont.Size = ToastFontSize;
        AchievementNameText->SetFont(NameFont);
    }
    if (CongratulationText)
    {
        CongratulationText->SetText(GetCongratulationText(InResult));
        CongratulationText->SetClipping(EWidgetClipping::ClipToBoundsAlways);
        CongratulationText->SetAutoWrapText(false);
        FSlateFontInfo CongratsFont = CongratulationText->GetFont();
        CongratsFont.Size = ToastFontSize;
        CongratulationText->SetFont(CongratsFont);
    }
    // 图标尺寸对齐 Unity prefab（Icon SizeDelta: 75×75）
    if (IconImage)
    {
        FSlateBrush Brush = IconImage->GetBrush();
        Brush.ImageSize = FVector2D(75.f, 75.f);
        IconImage->SetBrush(Brush);
    }
    // IconImage 固定 T_Achievement_Icon，已在 Designer 设死，无需在此赋值。

    // 2. 启动程序化动画（对照 Unity animation.Play("TapAchievementToastAC")）
    StartToastAnimation();

    // 3. 可选 BP 扩展钩子（默认空实现）
    OnSetAchievementData(InResult);
}

void UTapAchievementToastWidget::StartToastAnimation()
{
    SetVisibility(ESlateVisibility::Visible);
    // 使用纯 Opacity 淡入淡出动画，避免对全屏 Canvas Widget 做 RenderScale：
    // Scale 从 0→1 时以视口中心为轴，卡片在顶部会产生漂移/幽灵残影。
    // 对照 Unity：卡片本身做 Scale，但 UE WBP 根节点是全屏 Canvas，效果不同。
    SetRenderScale(FVector2D(1.f, 1.f));
    SetRenderOpacity(0.f);
    AnimElapsed  = 0.f;
    bAnimPlaying = true;
}

void UTapAchievementToastWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bAnimPlaying)
    {
        return;
    }

    AnimElapsed += InDeltaTime;

    const float HoldEnd  = ExpandDuration + HoldDuration;
    const float TotalEnd = HoldEnd + CollapseDuration;

    float Factor;  // 0 = 完全隐藏，1 = 完全显示
    if (AnimElapsed <= ExpandDuration)
    {
        // 展开：0 → 1（ease-out）
        const float Alpha = FMath::Clamp(AnimElapsed / ExpandDuration, 0.f, 1.f);
        Factor = FMath::InterpEaseOut(0.f, 1.f, Alpha, 2.f);
    }
    else if (AnimElapsed <= HoldEnd)
    {
        Factor = 1.f;  // 停留 3s
    }
    else if (AnimElapsed < TotalEnd)
    {
        // 收缩：1 → 0（ease-in）
        const float Alpha = FMath::Clamp((AnimElapsed - HoldEnd) / CollapseDuration, 0.f, 1.f);
        Factor = FMath::InterpEaseIn(1.f, 0.f, Alpha, 2.f);
    }
    else
    {
        // 动画结束 → 收尾并通知队列显示下一条（对照 Unity OnPlayEnd）
        bAnimPlaying = false;
        SetRenderOpacity(0.f);
        NotifyAnimationFinished();
        return;
    }

    SetRenderOpacity(Factor);
    // Scale 保持 1：用纯 opacity 动画，避免全屏 Canvas scale 产生幽灵残影
}

FReply UTapAchievementToastWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                           const FPointerEvent& InMouseEvent)
{
    // 对照 Unity TapAchievementToast.OnClick() → ShowAchievements()
    OnToastClicked();
    return FReply::Handled();
}

void UTapAchievementToastWidget::NotifyAnimationFinished()
{
#if PLATFORM_WINDOWS || PLATFORM_MAC
    FTapAchievementToastManager::Get().OnToastFinished();
#endif
}

void UTapAchievementToastWidget::OnToastClicked()
{
    // 对照 Unity TapAchievementToast.OnClick() → TapTapAchievement.ShowAchievements()
    UTapAchievement::ShowAchievements();
}

// 对照 Unity TapAchievementToast：
//   普通成就 → "恭喜解锁新成就"
//   白金成就 → "恭喜解锁白金成就"
FText UTapAchievementToastWidget::GetCongratulationText(const FTapAchievementResult& Result)
{
    if (Result.AchievementType == ETapAchievementType::Platinum)
    {
        return NSLOCTEXT("TapAchievement", "CongratsPlatinum", "恭喜解锁白金成就");
    }
    return NSLOCTEXT("TapAchievement", "CongratsNormal", "恭喜解锁新成就");
}

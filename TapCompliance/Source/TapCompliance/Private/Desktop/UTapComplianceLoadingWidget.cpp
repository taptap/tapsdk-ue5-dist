// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceLoadingWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Brushes/SlateColorBrush.h"

namespace
{
	/** 当前显示的 Loading 根 SWidget (单例) */
	TSharedPtr<SWidget> GLoadingRoot;
	TWeakObjectPtr<UGameViewportClient> GAttachedViewport;

	/** 半透明黑底蒙层颜色: alpha=0.5 */
	const FSlateColorBrush MaskBrush(FLinearColor(0.f, 0.f, 0.f, 0.5f));
}

void UTapComplianceLoadingWidget::ShowLoading(UObject* WorldContextObject)
{
	if (GLoadingRoot.IsValid())
	{
		// 已显示, no-op
		return;
	}
	if (!GEngine)
	{
		return;
	}

	UGameViewportClient* Viewport = GEngine->GameViewport;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Loading] No GameViewport — skipping loading UI"));
		return;
	}

	// 纯 Slate 构造:
	//   全屏 SBorder (半透明黑底, 阻止穿透点击)
	//     └── 居中 SCircularThrobber (UE 内置自动旋转, 不需要 Tick / 蓝图动画)
	TSharedRef<SWidget> Root = SNew(SBorder)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.BorderImage(&MaskBrush)
		.Padding(0)
		// SBorder 默认 IsHitTestVisible=true, 自然吸收点击, 阻止穿透到 game HUD
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(SCircularThrobber)
				.Radius(24.0f)
				.NumPieces(8)
				.Period(0.75f)
			]
		];

	// ZOrder 500 比业务面板 (1000) 低, 业务面板出现后视觉上盖住,
	// HideLoading 也会主动 RemoveViewportWidgetContent.
	Viewport->AddViewportWidgetContent(Root, /*ZOrder=*/500);

	GLoadingRoot       = Root;
	GAttachedViewport  = Viewport;

	UE_LOG(LogTapCompliance, Log, TEXT("[Loading] Shown (pure Slate)"));
}

void UTapComplianceLoadingWidget::HideLoading()
{
	if (!GLoadingRoot.IsValid())
	{
		return;
	}

	if (UGameViewportClient* Viewport = GAttachedViewport.Get())
	{
		Viewport->RemoveViewportWidgetContent(GLoadingRoot.ToSharedRef());
	}
	// 若 viewport 已失效 (例如 PIE 切换), TSharedPtr Reset 让 Slate 资源回收即可

	GLoadingRoot.Reset();
	GAttachedViewport.Reset();

	UE_LOG(LogTapCompliance, Log, TEXT("[Loading] Hidden"));
}

bool UTapComplianceLoadingWidget::IsShowing()
{
	return GLoadingRoot.IsValid();
}

#else // 非桌面平台: 空 stub

void UTapComplianceLoadingWidget::ShowLoading(UObject*) {}
void UTapComplianceLoadingWidget::HideLoading() {}
bool UTapComplianceLoadingWidget::IsShowing() { return false; }

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceToast.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Containers/Ticker.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateColorBrush.h"

namespace
{
	/** 当前显示的 Toast 根 SWidget (单例) */
	TSharedPtr<SWidget> GToastRoot;
	TWeakObjectPtr<UGameViewportClient> GAttachedViewport;
	FTSTicker::FDelegateHandle GAutoHideHandle;

	/**
	 * 直角矩形黑底 Toast 背景:
	 *   - 纯黑 alpha 0.88, 与系统级 toast (Android/iOS native) 配色一致
	 *   - 用 FSlateColorBrush 扁平色块, SBorder.BorderImage 原生支持, 渲染最稳定
	 */
	const FSlateColorBrush ToastBgBrush(FLinearColor(0.f, 0.f, 0.f, 0.88f));

	void DoHide()
	{
		if (GAutoHideHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(GAutoHideHandle);
			GAutoHideHandle.Reset();
		}
		if (!GToastRoot.IsValid())
		{
			return;
		}
		if (UGameViewportClient* Viewport = GAttachedViewport.Get())
		{
			Viewport->RemoveViewportWidgetContent(GToastRoot.ToSharedRef());
		}
		GToastRoot.Reset();
		GAttachedViewport.Reset();
		UE_LOG(LogTapCompliance, Log, TEXT("[Toast] Hidden"));
	}
}

void UTapComplianceToast::ShowToast(
	UObject* WorldContextObject,
	const FString& Message,
	float DurationSeconds)
{
	// 替换旧的 toast (短时间内多次提示时只显示最新一条)
	DoHide();

	if (!GEngine || !GEngine->GameViewport)
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Toast] No GameViewport — toast skipped: '%s'"), *Message);
		return;
	}
	UGameViewportClient* Viewport = GEngine->GameViewport;

	// 纯 Slate 构造:
	//   外层 SBorder (NoBrush 全屏定位容器, HitTestInvisible 不挡输入)
	//     └── 底部居中 SBorder (黑底, Padding 24x14)
	//          └── STextBlock (白字, 动态文案)
	TSharedRef<SWidget> Root = SNew(SBorder)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(FMargin(0.f, 0.f, 0.f, 80.f))
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("NoBrush")))
		.Visibility(EVisibility::HitTestInvisible)
		[
			SNew(SBorder)
			.BorderImage(&ToastBgBrush)
			.Padding(FMargin(24.f, 14.f))
			[
				SNew(STextBlock)
				.Text(FText::FromString(Message))
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
			]
		];

	// ZOrder 1100 > 业务面板 (1000), 确保 toast 浮在最上层
	Viewport->AddViewportWidgetContent(Root, /*ZOrder=*/1100);

	GToastRoot        = Root;
	GAttachedViewport = Viewport;

	if (DurationSeconds > 0.f)
	{
		GAutoHideHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([](float) -> bool
			{
				DoHide();
				return false; // 单次触发
			}),
			DurationSeconds);
	}

	UE_LOG(LogTapCompliance, Log,
		TEXT("[Toast] Shown: '%s' duration=%.2fs"), *Message, DurationSeconds);
}

void UTapComplianceToast::HideToast()
{
	DoHide();
}

#else // 非桌面平台: 空 stub

void UTapComplianceToast::ShowToast(UObject*, const FString&, float) {}
void UTapComplianceToast::HideToast() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

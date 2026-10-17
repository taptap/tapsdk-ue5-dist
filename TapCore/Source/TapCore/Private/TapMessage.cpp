// Copyright TapTap. All Rights Reserved.

#include "TapMessage.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapMessage, Log, All);

/** 最大延迟重试次数，防止 Viewport 始终不就绪时无限重试 */
static constexpr int32 MaxDeferRetries = 10;
/** 延迟重试间隔（秒） */
static constexpr float DeferIntervalSeconds = 0.5f;

void FTapMessage::ShowMessage(
	const FString& Msg,
	EPosition Position,
	EDuration Duration)
{
	ShowMessageInternal(Msg, Position, Duration, 0);
}

void FTapMessage::ShowMessageInternal(
	const FString& Msg,
	EPosition Position,
	EDuration Duration,
	int32 DeferCount)
{
	// 必须在 GameThread 执行
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [Msg, Position, Duration, DeferCount]()
		{
			ShowMessageInternal(Msg, Position, Duration, DeferCount);
		});
		return;
	}

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		// Viewport 尚未就绪 — 延迟重试，确保 gatekeeper 回调的 Toast 不丢失
		if (DeferCount < MaxDeferRetries)
		{
			UE_LOG(LogTapMessage, Log, TEXT("[TapMessage] Viewport not ready, deferring (%d/%d): %s"),
				DeferCount + 1, MaxDeferRetries, *Msg);

			FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateLambda([Msg, Position, Duration, DeferCount](float) -> bool
				{
					ShowMessageInternal(Msg, Position, Duration, DeferCount + 1);
					return false;
				}),
				DeferIntervalSeconds
			);
		}
		else
		{
			// 达到重试上限，降级为日志
			UE_LOG(LogTapMessage, Warning, TEXT("[TapMessage] Viewport unavailable after retries: %s"), *Msg);
		}
		return;
	}

	float Seconds = 2.0f;
	switch (Duration)
	{
	case EDuration::OneSecond:   Seconds = 1.0f; break;
	case EDuration::TwoSecond:   Seconds = 2.0f; break;
	case EDuration::ThreeSecond: Seconds = 3.0f; break;
	}

	EVerticalAlignment VAlign = (Position == EPosition::Top) ? VAlign_Top : VAlign_Bottom;
	FMargin OuterPadding = (Position == EPosition::Top)
		? FMargin(0, 100, 0, 0)
		: FMargin(0, 0, 0, 100);

	// 对齐 Unity TapSDKCommonToastBg.png 样式:
	// - 深灰黑色胶囊形圆角矩形（sprite 颜色约 RGB(0.15, 0.15, 0.15)）
	// - 圆角半径 21（sprite 高 42px，完全圆角）
	// - 内边距: left=25, top=10, right=25, bottom=10
	// - 白色文字，字号对齐 Unity 11pt（换算 UE Slate ≈ 14）
	static FSlateRoundedBoxBrush ToastBrush(
		FLinearColor(0.15f, 0.15f, 0.15f, 0.9f),  // 深灰黑色，90% 不透明
		21.0f                                       // 圆角半径，对齐 sprite 高度 42/2
	);

	TSharedRef<SWidget> Toast =
		SNew(SOverlay)
		+ SOverlay::Slot()
		.VAlign(VAlign)
		.HAlign(HAlign_Center)
		.Padding(OuterPadding)
		[
			SNew(SBorder)
			.BorderImage(&ToastBrush)
			.Padding(FMargin(25.f, 10.f))  // 对齐 Unity LayoutGroup padding: L25 T10 R25 B10
			[
				SNew(STextBlock)
				.Text(FText::FromString(Msg))
				.ColorAndOpacity(FSlateColor(FLinearColor::White))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
				.Justification(ETextJustify::Center)
			]
		];

	Viewport->AddViewportWidgetContent(Toast, 10000);

	UE_LOG(LogTapMessage, Log, TEXT("[TapMessage] %s"), *Msg);

	// 定时移除 — 对齐 Unity Destroy(clone, 2f)
	TWeakObjectPtr<UGameViewportClient> WeakViewport(Viewport);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakViewport, Toast](float) -> bool
		{
			if (UGameViewportClient* VP = WeakViewport.Get())
			{
				VP->RemoveViewportWidgetContent(Toast);
			}
			return false; // 只执行一次
		}),
		Seconds
	);
}

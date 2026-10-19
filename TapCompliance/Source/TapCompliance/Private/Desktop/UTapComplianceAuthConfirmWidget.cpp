// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceAuthConfirmWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"

// UMG
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "Containers/Ticker.h"     // FTSTicker — 延后到下一帧 SetText, 避开 NativeConstruct 时机

UTapComplianceAuthConfirmWidget::UTapComplianceAuthConfirmWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// 公共接口
// =============================================================================

void UTapComplianceAuthConfirmWidget::SetUserInfo(const FString& InMaskedName, const FString& InMaskedIDNumber)
{
	MaskedName     = InMaskedName;
	MaskedIDNumber = InMaskedIDNumber;

	// 方案 B: 业务两个字段都空就完全不动 UTextBlock, 让 WBP 默认占位文本显示.
	// 任意一个有值就标记 custom, 等构造完成后渲染.
	bHasCustomUserInfo = !InMaskedName.IsEmpty() || !InMaskedIDNumber.IsEmpty();
	if (!bHasCustomUserInfo)
	{
		return;
	}

	if (IsConstructed())
	{
		RefreshUserInfoText();
	}
}

void UTapComplianceAuthConfirmWidget::Init(
	FOnAuthConfirmConfirmed InOnConfirmed,
	FOnAuthConfirmClosed InOnClosed)
{
	OnConfirmed = InOnConfirmed;
	OnClosed    = InOnClosed;
}

// =============================================================================
// NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceAuthConfirmWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UTapComplianceAuthConfirmWidget::OnConfirmClicked);
	}

	// Image_Close 没有 OnClicked, 走 OnMouseButtonDownEvent
	// (WBP 里 Visibility 须为 Visible, Hit Test Invisible 不接收点击)
	if (Image_Close)
	{
		Image_Close->OnMouseButtonDownEvent.BindDynamic(this, &UTapComplianceAuthConfirmWidget::OnImageClosePressed);
	}

	// 方案 B: 默认不调 SetText, 让 WBP 设计稿里的占位文本 (空字符串或 "***") 显示.
	// 直接在 NativeConstruct 调 SetText 会让 UTextBlock 内部状态进入半初始化, GC 析构时崩
	// (与 FastAuth URichTextBlock 同源问题, callstack 都是 UMG vector deleting destructor +
	// 读 0xFFFFFFFFFFFFFFFF). 业务侧传了数据时延后到下一帧再 SetText.
	if (bHasCustomUserInfo)
	{
		TWeakObjectPtr<UTapComplianceAuthConfirmWidget> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float) -> bool
			{
				if (UTapComplianceAuthConfirmWidget* Self = WeakThis.Get())
				{
					Self->RefreshUserInfoText();
				}
				return false; // 单次触发
			}),
			0.0f);
	}
}

void UTapComplianceAuthConfirmWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 文本渲染
// =============================================================================

void UTapComplianceAuthConfirmWidget::RefreshUserInfoText()
{
	// label ("• 真实姓名:") 与圆点都在蓝图里写死, 这里只把脱敏后的 value 注入对应 TextBlock.
	// 这样 label 与 value 可以在蓝图侧分别配色 (label #52575D / value #1D2127),
	// 不需要 RichTextBlock + style run.
	if (TextBlock_Name)
	{
		TextBlock_Name->SetText(FText::FromString(MaskedName));
	}
	if (TextBlock_IDNumber)
	{
		TextBlock_IDNumber->SetText(FText::FromString(MaskedIDNumber));
	}
}

// =============================================================================
// 按钮事件
// =============================================================================

FEventReply UTapComplianceAuthConfirmWidget::OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[AuthConfirm] Image_Close pressed"));
	OnClosed.ExecuteIfBound();
	RemoveFromParent();

	FEventReply Reply;
	Reply.NativeReply = FReply::Handled();
	return Reply;
}

void UTapComplianceAuthConfirmWidget::OnConfirmClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[AuthConfirm] Confirm clicked -> notify confirmed"));
	// 不在此处 RemoveFromParent: 由上层 OnConfirmed 决定后续流程
	// (例如 PanelManager 在跳转到下一面板时通过 CloseAllPanels 销毁本面板),
	// 避免重复销毁.
	OnConfirmed.ExecuteIfBound();
}

#else // 非桌面平台: 提供空 stub 以满足 UHT 生成代码链接

UTapComplianceAuthConfirmWidget::UTapComplianceAuthConfirmWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceAuthConfirmWidget::SetUserInfo(const FString&, const FString&) {}
void UTapComplianceAuthConfirmWidget::Init(
	FOnAuthConfirmConfirmed, FOnAuthConfirmClosed) {}

void UTapComplianceAuthConfirmWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceAuthConfirmWidget::NativeDestruct()  { Super::NativeDestruct();  }

FEventReply UTapComplianceAuthConfirmWidget::OnImageClosePressed(FGeometry, const FPointerEvent&) { return {}; }
void UTapComplianceAuthConfirmWidget::OnConfirmClicked() {}
void UTapComplianceAuthConfirmWidget::RefreshUserInfoText() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

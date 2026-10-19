// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceAuthReviewingWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"
#include "ComplianceModels.h"
#include "ComplianceState.h"

// UMG
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "Containers/Ticker.h"     // FTSTicker — 延后到下一帧 SetText, 避开 NativeConstruct 时机

UTapComplianceAuthReviewingWidget::UTapComplianceAuthReviewingWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// 公共接口
// =============================================================================

void UTapComplianceAuthReviewingWidget::Init(
	EAuthReviewingState InState,
	FOnAuthReviewingConfirmed InOnConfirmed,
	FOnAuthReviewingClosed InOnClosed)
{
	State       = InState;
	OnConfirmed = InOnConfirmed;
	OnClosed    = InOnClosed;
}

// =============================================================================
// NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceAuthReviewingWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UTapComplianceAuthReviewingWidget::OnConfirmClicked);
	}

	// Image_Close 没有 OnClicked, 走 OnMouseButtonDownEvent
	// (WBP 里 Visibility 须为 Visible, Hit Test Invisible 不接收点击)
	if (Image_Close)
	{
		Image_Close->OnMouseButtonDownEvent.BindDynamic(this, &UTapComplianceAuthReviewingWidget::OnImageClosePressed);
	}

	// 服务端 AuthWaitingTip 文案动态注入: 延后到下一帧, 避开 NativeConstruct 期间
	// SetText URichTextBlock 引发的析构野指针 (与 FastAuth/AuthConfirm 同源).
	// 若 RealNameConfig 整体未抓到或字段空, ApplyServerPromptText 内部会逐字段判空,
	// 全空时不动任何控件 → WBP 默认占位文案显示.
	TWeakObjectPtr<UTapComplianceAuthReviewingWidget> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float) -> bool
		{
			if (UTapComplianceAuthReviewingWidget* Self = WeakThis.Get())
			{
				Self->ApplyServerPromptText();
			}
			return false; // 单次触发
		}),
		0.0f);
}

void UTapComplianceAuthReviewingWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 服务端文案注入
// =============================================================================

void UTapComplianceAuthReviewingWidget::ApplyServerPromptText()
{
	// 按 State 选 prompt 源 (对齐 Android RealNameVerifyDialog "waiting" / "failed" 分支)
	const FRealNameText& Texts = FComplianceState::Get().RealNameConfig.RealNameText;
	const FCompliancePrompt& Prompt = (State == EAuthReviewingState::Failed)
		? Texts.ManualAuthFailedTip
		: Texts.AuthWaitingTip;

	// 方案 B: 字段空就完全不调 SetText, 让 WBP 默认占位文案保留.
	if (TextBlock_Title && !Prompt.Title.IsEmpty())
	{
		TextBlock_Title->SetText(FText::FromString(Prompt.Title));
	}
	if (TextBlock_Description && !Prompt.Content.IsEmpty())
	{
		// description_plain 是纯文本, 无 HTML/富文本解析
		TextBlock_Description->SetText(FText::FromString(Prompt.Content));
	}
	if (TextBlock_ConfirmLabel && !Prompt.PositiveButtonText.IsEmpty())
	{
		TextBlock_ConfirmLabel->SetText(FText::FromString(Prompt.PositiveButtonText));
	}
}

// =============================================================================
// 按钮事件
// =============================================================================

FEventReply UTapComplianceAuthReviewingWidget::OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[AuthReviewing] Image_Close pressed"));
	OnClosed.ExecuteIfBound();
	RemoveFromParent();

	FEventReply Reply;
	Reply.NativeReply = FReply::Handled();
	return Reply;
}

void UTapComplianceAuthReviewingWidget::OnConfirmClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[AuthReviewing] Confirm clicked -> notify confirmed"));
	// 纯告知性弹窗, 主按钮点击后自行 dismiss; 上层通常借 OnConfirmed 触发
	// Finish(REAL_NAME_STOP) 或其他业务收尾.
	OnConfirmed.ExecuteIfBound();
	RemoveFromParent();
}

#else // 非桌面平台: 提供空 stub 以满足 UHT 生成代码链接

UTapComplianceAuthReviewingWidget::UTapComplianceAuthReviewingWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceAuthReviewingWidget::Init(
	EAuthReviewingState, FOnAuthReviewingConfirmed, FOnAuthReviewingClosed) {}

void UTapComplianceAuthReviewingWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceAuthReviewingWidget::NativeDestruct()  { Super::NativeDestruct();  }

FEventReply UTapComplianceAuthReviewingWidget::OnImageClosePressed(FGeometry, const FPointerEvent&) { return {}; }
void UTapComplianceAuthReviewingWidget::OnConfirmClicked() {}
void UTapComplianceAuthReviewingWidget::ApplyServerPromptText() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

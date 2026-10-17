// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceFastAuthWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"
#include "ComplianceModels.h"
#include "ComplianceState.h"

// UMG
#include "Components/Button.h"
#include "UTapComplianceEditBox.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Input/Reply.h"
#include "Containers/Ticker.h"     // FTSTicker — 延后到下一帧 SetText, 避开 NativeConstruct 时机

namespace
{
	// 输入框配色 (设计稿)
	//   Hint  : #B9BEC1  - 占位提示文字
	//   Typed : #52575D  - 用户输入后的正文颜色
	// UE 5.x 的 SEditableTextBox 没有独立的 HintColor 属性, hint 与输入文字共用
	// ForegroundColor; 所以这里通过监听 OnTextChanged, 在空 / 非空之间切换颜色实现.
	const FLinearColor InputHintColor  = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("B9BEC1")));
	const FLinearColor InputTypedColor = FLinearColor::FromSRGBColor(FColor::FromHex(TEXT("52575D")));

	void RefreshInputForegroundColor(UTapComplianceEditBox* Input)
	{
		if (!Input) return;
		Input->ApplyForegroundColor(Input->GetText().IsEmpty() ? InputHintColor : InputTypedColor);
	}
}

UTapComplianceFastAuthWidget::UTapComplianceFastAuthWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// 公共接口
// =============================================================================

void UTapComplianceFastAuthWidget::Init(
	FOnFastAuthConfirmed InOnConfirmed,
	FOnFastAuthCancelled InOnCancelled,
	FOnFastAuthClosed    InOnClosed,
	bool                 bInShowCancelButton)
{
	OnConfirmed        = InOnConfirmed;
	OnCancelled        = InOnCancelled;
	OnClosed           = InOnClosed;
	bShowCancelButton  = bInShowCancelButton;
}

// =============================================================================
// NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceFastAuthWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 绑定按钮事件
	if (Button_Cancel)  Button_Cancel ->OnClicked.AddDynamic(this, &UTapComplianceFastAuthWidget::OnCancelClicked);
	if (Button_Confirm) Button_Confirm->OnClicked.AddDynamic(this, &UTapComplianceFastAuthWidget::OnConfirmClicked);

	// 调用方已按 Android/Unity 语义算好是否显示"不使用":
	// manual_auth_enable=false 或 Tap 已登录但缺 compliance scope 时隐藏.
	if (Button_Cancel && !bShowCancelButton)
	{
		Button_Cancel->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Image_Close 是 UImage, 没有 OnClicked, 用 OnMouseButtonDownEvent 接收点击.
	// 注意: WBP 里 Image_Close 的 Visibility 必须是 Visible (默认就是), 否则不接收点击事件.
	if (Image_Close)
	{
		Image_Close->OnMouseButtonDownEvent.BindDynamic(this, &UTapComplianceFastAuthWidget::OnImageClosePressed);
	}

	// 输入框配色: hint #B9BEC1 / 输入文字 #52575D
	// 通过 OnTextChanged 在空与非空之间切换 ForegroundColor
	if (NameInput)
	{
		NameInput->OnTextChanged.AddDynamic(this, &UTapComplianceFastAuthWidget::OnNameInputTextChanged);
		RefreshInputForegroundColor(NameInput);
	}
	if (IDNumberInput)
	{
		IDNumberInput->OnTextChanged.AddDynamic(this, &UTapComplianceFastAuthWidget::OnIDNumberInputTextChanged);
		RefreshInputForegroundColor(IDNumberInput);
	}

	// 服务端文案延后到下一帧 SetText, 避开 NativeConstruct 期间 SetText 引发的
	// 析构野指针 (callstack 都是 UMG vector deleting destructor + 读 0xFF..FF, 历史上
	// 在 URichTextBlock 上出现, 防御性保留延后范式).
	// TapAuthTip 字段空时 ApplyServerPromptText 内部跳过 SetText → 保留 WBP 默认占位文案
	// (含 TextBlock_Description 的设计稿默认正文, 不再做本地 GameName 模板兜底).
	TWeakObjectPtr<UTapComplianceFastAuthWidget> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float) -> bool
		{
			if (UTapComplianceFastAuthWidget* Self = WeakThis.Get())
			{
				Self->ApplyServerPromptText();
			}
			return false; // 单次触发
		}),
		0.0f); // 下一帧
}

void UTapComplianceFastAuthWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 正文渲染
// =============================================================================

// =============================================================================
// 服务端文案注入
// =============================================================================

void UTapComplianceFastAuthWidget::ApplyServerPromptText()
{
	const FCompliancePrompt& Prompt =
		FComplianceState::Get().RealNameConfig.RealNameText.TapAuthTip;

	// 方案 B: 字段空就完全不调 SetText, 让 WBP 默认占位文案保留 (含 Body 默认正文,
	// 不再做本地 GameName 模板兜底). description_plain 是纯文本, 直接 SetText 即可.
	if (TextBlock_Title && !Prompt.Title.IsEmpty())
	{
		TextBlock_Title->SetText(FText::FromString(Prompt.Title));
	}
	if (TextBlock_Description && !Prompt.Content.IsEmpty())
	{
		// description_plain 纯文本, 无 HTML/富文本解析
		TextBlock_Description->SetText(FText::FromString(Prompt.Content));
	}
	if (TextBlock_ConfirmLabel && !Prompt.PositiveButtonText.IsEmpty())
	{
		TextBlock_ConfirmLabel->SetText(FText::FromString(Prompt.PositiveButtonText));
	}
	if (TextBlock_CancelLabel && !Prompt.NegativeButtonText.IsEmpty())
	{
		TextBlock_CancelLabel->SetText(FText::FromString(Prompt.NegativeButtonText));
	}
}

// =============================================================================
// 按钮事件 (业务逻辑暂未接入, 预留委托回调点)
// =============================================================================

FEventReply UTapComplianceFastAuthWidget::OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[FastAuth] Image_Close pressed"));
	OnClosed.ExecuteIfBound();
	RemoveFromParent();

	FEventReply Reply;
	Reply.NativeReply = FReply::Handled();   // 标记事件已处理, 阻止继续向上冒泡
	return Reply;
}

void UTapComplianceFastAuthWidget::OnCancelClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[FastAuth] Cancel clicked -> notify cancelled"));
	// 对齐 Unity ComplianceWorker.InternalGetAccessToken: 拒绝快速认证 → 抛 "refuse quick verify"
	// → 外层 catch 设 mannualVerify=1 → VerifyManuallyAsync. 这里通过委托交给 Flow 层走 ShowRealNameManual.
	OnCancelled.ExecuteIfBound();
	RemoveFromParent();
}

void UTapComplianceFastAuthWidget::OnConfirmClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[FastAuth] Confirm clicked -> notify confirmed"));
	// Align Unity TapTapComplianceQuickVerifyTipController: close the prompt before
	// invoking the business callback. FastAuth success may continue without opening
	// another panel, so relying on CloseAllPanels would leave this widget on screen.
	RemoveFromParent();
	OnConfirmed.ExecuteIfBound();
}

void UTapComplianceFastAuthWidget::OnNameInputTextChanged(const FText& Text)
{
	RefreshInputForegroundColor(NameInput);
}

void UTapComplianceFastAuthWidget::OnIDNumberInputTextChanged(const FText& Text)
{
	RefreshInputForegroundColor(IDNumberInput);
}

#else // 非桌面平台: 提供空 stub 以满足 UHT 生成代码链接

UTapComplianceFastAuthWidget::UTapComplianceFastAuthWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceFastAuthWidget::Init(
	FOnFastAuthConfirmed,
	FOnFastAuthCancelled,
	FOnFastAuthClosed,
	bool) {}

void UTapComplianceFastAuthWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceFastAuthWidget::NativeDestruct()  { Super::NativeDestruct();  }

FEventReply UTapComplianceFastAuthWidget::OnImageClosePressed(FGeometry, const FPointerEvent&) { return {}; }
void UTapComplianceFastAuthWidget::OnCancelClicked()  {}
void UTapComplianceFastAuthWidget::OnConfirmClicked() {}
void UTapComplianceFastAuthWidget::OnNameInputTextChanged(const FText&) {}
void UTapComplianceFastAuthWidget::OnIDNumberInputTextChanged(const FText&) {}
void UTapComplianceFastAuthWidget::ApplyServerPromptText() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

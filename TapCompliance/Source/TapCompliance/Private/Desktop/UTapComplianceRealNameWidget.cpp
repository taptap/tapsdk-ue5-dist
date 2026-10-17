// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceRealNameWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"
#include "ComplianceModels.h"
#include "ComplianceState.h"
#include "UTapComplianceToast.h"

// UMG
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "UTapComplianceEditBox.h"
#include "UTapComplianceNumericEditBox.h"
#include "Input/Reply.h"
#include "Containers/Ticker.h"     // FTSTicker — 延后到下一帧 SetText, 避开 NativeConstruct 时机

namespace
{
	// 输入框配色 (与 FastAuth 对齐)
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

UTapComplianceRealNameWidget::UTapComplianceRealNameWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// Init / NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceRealNameWidget::Init(
	FOnCompliancePanelClose InOnClose,
	FOnCompliancePanelBack InOnBack,
	FOnCompliancePanelSubmit InOnSubmit,
	bool bInHasAuthRecord)
{
	OnClose = InOnClose;
	OnBack = InOnBack;
	OnSubmit = InOnSubmit;
	bSubmitInProgress = false;
	bHasSubmittedInfo = false;
	LastSubmittedName.Empty();
	LastSubmittedIDNumber.Empty();
	bHasAuthRecord = bInHasAuthRecord;
}

void UTapComplianceRealNameWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 诊断: BindWidgetOptional 实际绑定状态. nullptr 说明 WBP 里同名控件不存在 / 类型不匹配,
	// SubmitBtn 不绑会导致点击无回调; NameInput/IDNumberInput 不绑会导致 UpdateSubmitButtonState 永远 disable 按钮.
	UE_LOG(LogTapCompliance, Log,
		TEXT("[RealName] BindWidget status: SubmitBtn=%s NameInput=%s IDNumberInput=%s Image_Close=%s"),
		SubmitBtn      ? TEXT("BOUND") : TEXT("NULL"),
		NameInput      ? TEXT("BOUND") : TEXT("NULL"),
		IDNumberInput  ? TEXT("BOUND") : TEXT("NULL"),
		Image_Close    ? TEXT("BOUND") : TEXT("NULL"));

	// 绑定按钮事件 (WBP 仅有 SubmitBtn; 关闭入口走 Image_Close 的 OnMouseButtonDown)
	if (SubmitBtn) SubmitBtn->OnClicked.AddDynamic(this, &UTapComplianceRealNameWidget::OnSubmitClicked);

	// Image_Close 是 UImage, 没有 OnClicked, 用 OnMouseButtonDownEvent 接收点击.
	// 注意: WBP 里 Image_Close 的 Visibility 必须是 Visible (默认就是), 否则不接收点击事件.
	if (Image_Close)
	{
		Image_Close->OnMouseButtonDownEvent.BindDynamic(this, &UTapComplianceRealNameWidget::OnImageClosePressed);
	}

	// 绑定输入框文字变化事件, 并按空/非空切换 hint gray / 输入正文色
	if (NameInput)
	{
		NameInput->OnTextChanged.AddDynamic(this, &UTapComplianceRealNameWidget::OnNameInputChanged);
		RefreshInputForegroundColor(NameInput);
	}
	if (IDNumberInput)
	{
		IDNumberInput->OnTextChanged.AddDynamic(this, &UTapComplianceRealNameWidget::OnIDNumberInputChanged);
		RefreshInputForegroundColor(IDNumberInput);
	}

	// 初始化 UI 状态
	HideError();
	UpdateSubmitButtonState();

	// 服务端文案动态注入: 延后到下一帧, 避开 NativeConstruct 期间 SetText
	// URichTextBlock 引发的析构野指针 (与 FastAuth/AuthConfirm/AuthReviewing 同源).
	TWeakObjectPtr<UTapComplianceRealNameWidget> WeakThis(this);
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([WeakThis](float) -> bool
		{
			if (UTapComplianceRealNameWidget* Self = WeakThis.Get())
			{
				Self->ApplyServerPromptText();
			}
			return false; // 单次触发
		}),
		0.0f);
}

void UTapComplianceRealNameWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 服务端文案注入
// =============================================================================

void UTapComplianceRealNameWidget::ApplyServerPromptText()
{
	// 对齐 Android RealNameVerifyDialog.kt:84-96 hasAuthRecord 二选一
	const FRealNameText& Texts = FComplianceState::Get().RealNameConfig.RealNameText;
	const FCompliancePrompt& Prompt = bHasAuthRecord
		? Texts.ManualAuthFailedTip
		: Texts.ManualAuthTip;

	// 方案 B: 字段空就完全不调 SetText, 让 WBP 默认占位文案保留.
	if (TextBlock_Title && !Prompt.Title.IsEmpty())
	{
		TextBlock_Title->SetText(FText::FromString(Prompt.Title));
	}
	if (TextBlock_Description && !Prompt.Content.IsEmpty())
	{
		// description_plain 纯文本, 无 HTML/富文本解析
		TextBlock_Description->SetText(FText::FromString(Prompt.Content));
	}
	if (TextBlock_SubmitLabel && !Prompt.PositiveButtonText.IsEmpty())
	{
		TextBlock_SubmitLabel->SetText(FText::FromString(Prompt.PositiveButtonText));
	}
}

// =============================================================================
// 错误提示
// =============================================================================

void UTapComplianceRealNameWidget::ShowError(const FString& Message)
{
	UE_LOG(LogTapCompliance, Log,
		TEXT("[RealName] ShowError: msg='%s' ErrorHintText=%s ErrorHintArea=%s"),
		*Message,
		ErrorHintText ? TEXT("BOUND") : TEXT("NULL"),
		ErrorHintArea ? TEXT("BOUND") : TEXT("NULL"));

	const bool bHasInlineHint = (ErrorHintText != nullptr) || (ErrorHintArea != nullptr);
	if (bHasInlineHint)
	{
		// 优先用 WBP 内嵌错误条 (UX 更轻量, 不挡视线)
		if (ErrorHintText)
		{
			ErrorHintText->SetText(FText::FromString(Message));
		}
		if (ErrorHintArea)
		{
			ErrorHintArea->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		}
	}
	else
	{
		// WBP 无内嵌错误条 → 兜底弹顶部 Toast, 保证用户看到失败原因
		// (与 Unity TapTapComplianceUI.OpenToast(message, Error) 行为对齐)
		UTapComplianceToast::ShowToast(this, Message);
	}
}

void UTapComplianceRealNameWidget::HideError()
{
	if (ErrorHintArea)
	{
		ErrorHintArea->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// =============================================================================
// 输入框 & 按钮状态
// =============================================================================

FString UTapComplianceRealNameWidget::GetNormalizedNameInput() const
{
	FString Name = NameInput ? NameInput->GetText().ToString() : TEXT("");
	return Name.TrimStartAndEnd();
}

FString UTapComplianceRealNameWidget::GetNormalizedIDNumberInput() const
{
	FString IDNumber = IDNumberInput ? IDNumberInput->GetText().ToString() : TEXT("");
	return IDNumber.TrimStartAndEnd();
}

bool UTapComplianceRealNameWidget::IsDuplicateSubmittedInfo(
	const FString& Name,
	const FString& IDNumber) const
{
	return bHasSubmittedInfo
		&& Name.Equals(LastSubmittedName)
		&& IDNumber.Equals(LastSubmittedIDNumber);
}

void UTapComplianceRealNameWidget::UpdateSubmitButtonState()
{
	if (!SubmitBtn) return;

	const FString Name = GetNormalizedNameInput();
	const FString IDNumber = GetNormalizedIDNumberInput();
	const bool bHasName = !Name.IsEmpty();
	const bool bHasID = !IDNumber.IsEmpty();
	const bool bDuplicate = IsDuplicateSubmittedInfo(Name, IDNumber);
	const bool bEnable = true;
	SubmitBtn->SetIsEnabled(bEnable);
	UE_LOG(LogTapCompliance, Verbose,
		TEXT("[RealName] UpdateSubmitButtonState: enable=%s (hasName=%s hasID=%s inProgress=%s duplicate=%s)"),
		bEnable           ? TEXT("true") : TEXT("false"),
		bHasName          ? TEXT("true") : TEXT("false"),
		bHasID            ? TEXT("true") : TEXT("false"),
		bSubmitInProgress ? TEXT("true") : TEXT("false"),
		bDuplicate        ? TEXT("true") : TEXT("false"));
}

void UTapComplianceRealNameWidget::OnNameInputChanged(const FText& Text)
{
	HideError();
	UpdateSubmitButtonState();
	RefreshInputForegroundColor(NameInput);
}

void UTapComplianceRealNameWidget::OnIDNumberInputChanged(const FText& Text)
{
	HideError();
	UpdateSubmitButtonState();
	RefreshInputForegroundColor(IDNumberInput);
}

// =============================================================================
// 按钮事件
// =============================================================================

FEventReply UTapComplianceRealNameWidget::OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[RealName] Image_Close pressed"));
	OnClose.ExecuteIfBound();
	RemoveFromParent();

	FEventReply Reply;
	Reply.NativeReply = FReply::Handled();   // 标记事件已处理, 阻止继续向上冒泡
	return Reply;
}

void UTapComplianceRealNameWidget::OnSubmitClicked()
{
	// 入口诊断: 一旦执行说明 SubmitBtn 的 OnClicked 被触发 (按钮已 enabled 且绑定成功).
	UE_LOG(LogTapCompliance, Log,
		TEXT("[RealName] OnSubmitClicked entered (inProgress=%s)"),
		bSubmitInProgress ? TEXT("true") : TEXT("false"));

	if (bSubmitInProgress)
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[RealName] Submit ignored: request already in progress"));
		return;
	}

	const FString Name = GetNormalizedNameInput();
	const FString IDNumber = GetNormalizedIDNumberInput();

	// 分别校验空值, 给出精确提示 (姓名优先):
	//   姓名空 (无论身份信息是否同时为空) → "姓名不能为空"   (场景 1 / 场景 2)
	//   仅身份信息空                       → "身份信息不能为空" (场景 3)
	if (Name.IsEmpty())
	{
		ShowError(TEXT("姓名不能为空"));
		return;
	}
	if (IDNumber.IsEmpty())
	{
		ShowError(TEXT("身份信息不能为空"));
		return;
	}

	if (IsDuplicateSubmittedInfo(Name, IDNumber))
	{
		ShowError(TEXT("输入信息有误，请修改后重新提交"));
		UpdateSubmitButtonState();
		return;
	}

	bSubmitInProgress = true;
	bHasSubmittedInfo = true;
	LastSubmittedName = Name;
	LastSubmittedIDNumber = IDNumber;
	UpdateSubmitButtonState();
	HideError();

	UE_LOG(LogTapCompliance, Log, TEXT("[RealName] Submit: Name=%s, IDNumber=%s"), TEXT("[REDACTED]"), TEXT("[REDACTED]"));
	OnSubmit.ExecuteIfBound(Name, IDNumber);
}

void UTapComplianceRealNameWidget::NotifySubmitFailed(const FString& ErrorMessage)
{
	UE_LOG(LogTapCompliance, Log,
		TEXT("[RealName] NotifySubmitFailed: '%s'"), *ErrorMessage);
	bSubmitInProgress = false;
	UpdateSubmitButtonState();
	ShowError(ErrorMessage);
}

void UTapComplianceRealNameWidget::NotifySubmitSuccess()
{
	bSubmitInProgress = false;
	UE_LOG(LogTapCompliance, Log, TEXT("[RealName] Submit success"));
}

#else // Non-desktop platforms: empty stubs for UHT-generated code

UTapComplianceRealNameWidget::UTapComplianceRealNameWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceRealNameWidget::Init(
	FOnCompliancePanelClose InOnClose,
	FOnCompliancePanelBack InOnBack,
	FOnCompliancePanelSubmit InOnSubmit,
	bool bInHasAuthRecord)
{
}

void UTapComplianceRealNameWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceRealNameWidget::NativeDestruct() { Super::NativeDestruct(); }
void UTapComplianceRealNameWidget::OnSubmitClicked() {}
void UTapComplianceRealNameWidget::OnNameInputChanged(const FText& Text) {}
void UTapComplianceRealNameWidget::OnIDNumberInputChanged(const FText& Text) {}
FEventReply UTapComplianceRealNameWidget::OnImageClosePressed(FGeometry, const FPointerEvent&) { return {}; }
void UTapComplianceRealNameWidget::NotifySubmitFailed(const FString& ErrorMessage) {}
void UTapComplianceRealNameWidget::NotifySubmitSuccess() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

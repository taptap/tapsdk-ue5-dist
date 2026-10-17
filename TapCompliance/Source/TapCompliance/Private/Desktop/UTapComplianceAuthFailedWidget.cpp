// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceAuthFailedWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"

// UMG
#include "Components/Button.h"
#include "Components/Image.h"
#include "Input/Reply.h"

UTapComplianceAuthFailedWidget::UTapComplianceAuthFailedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// 公共接口
// =============================================================================

void UTapComplianceAuthFailedWidget::Init(
	FOnAuthFailedConfirmed InOnConfirmed,
	FOnAuthFailedClosed InOnClosed)
{
	OnConfirmed = InOnConfirmed;
	OnClosed    = InOnClosed;
}

// =============================================================================
// NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceAuthFailedWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddDynamic(this, &UTapComplianceAuthFailedWidget::OnConfirmClicked);
	}

	// Image_Close 没有 OnClicked, 走 OnMouseButtonDownEvent
	// (WBP 里 Visibility 须为 Visible, Hit Test Invisible 不接收点击)
	if (Image_Close)
	{
		Image_Close->OnMouseButtonDownEvent.BindDynamic(this, &UTapComplianceAuthFailedWidget::OnImageClosePressed);
	}
}

void UTapComplianceAuthFailedWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 按钮事件
// =============================================================================

FEventReply UTapComplianceAuthFailedWidget::OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[AuthFailed] Image_Close pressed"));
	OnClosed.ExecuteIfBound();
	RemoveFromParent();

	FEventReply Reply;
	Reply.NativeReply = FReply::Handled();
	return Reply;
}

void UTapComplianceAuthFailedWidget::OnConfirmClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[AuthFailed] Confirm clicked -> notify confirmed"));
	// 不在此处 RemoveFromParent: 由上层 OnConfirmed 决定后续流程
	// (典型场景是跳转到 RealNamePanel, PanelManager.ShowPanel 内部 CloseAllPanels
	//  会顺带销毁本面板).
	OnConfirmed.ExecuteIfBound();
}

#else // 非桌面平台: 提供空 stub 以满足 UHT 生成代码链接

UTapComplianceAuthFailedWidget::UTapComplianceAuthFailedWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceAuthFailedWidget::Init(
	FOnAuthFailedConfirmed, FOnAuthFailedClosed) {}

void UTapComplianceAuthFailedWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceAuthFailedWidget::NativeDestruct()  { Super::NativeDestruct();  }

FEventReply UTapComplianceAuthFailedWidget::OnImageClosePressed(FGeometry, const FPointerEvent&) { return {}; }
void UTapComplianceAuthFailedWidget::OnConfirmClicked() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

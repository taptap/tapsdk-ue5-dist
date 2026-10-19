// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceHealthAuthRequiredWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"

// UMG
#include "Components/Button.h"
#include "Components/Image.h"
#include "Input/Reply.h"

UTapComplianceHealthAuthRequiredWidget::UTapComplianceHealthAuthRequiredWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// 公共接口
// =============================================================================

void UTapComplianceHealthAuthRequiredWidget::Init(
	FOnHealthAuthRequiredSwitchAccount InOnSwitchAccount,
	FOnHealthAuthRequiredExitGame      InOnExitGame,
	FOnHealthAuthRequiredGoAuth        InOnGoAuth,
	FOnHealthAuthRequiredClosed        InOnClosed,
	bool                               bInShowSwitchAccount)
{
	OnSwitchAccount    = InOnSwitchAccount;
	OnExitGame         = InOnExitGame;
	OnGoAuth           = InOnGoAuth;
	OnClosed           = InOnClosed;
	bShowSwitchAccount = bInShowSwitchAccount;
}

// =============================================================================
// NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceHealthAuthRequiredWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_SwitchAccount) Button_SwitchAccount->OnClicked.AddDynamic(this, &UTapComplianceHealthAuthRequiredWidget::OnSwitchAccountClicked);
	if (Button_ExitGame)      Button_ExitGame     ->OnClicked.AddDynamic(this, &UTapComplianceHealthAuthRequiredWidget::OnExitGameClicked);
	if (Button_GoAuth)        Button_GoAuth       ->OnClicked.AddDynamic(this, &UTapComplianceHealthAuthRequiredWidget::OnGoAuthClicked);

	// 对齐 Android HealthReminderDialog (HealthReminderDialog.kt:84): bShowSwitchAccount=false 时隐藏切换账号按钮
	if (Button_SwitchAccount && !bShowSwitchAccount)
	{
		Button_SwitchAccount->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Image_Close 没有 OnClicked, 走 OnMouseButtonDownEvent
	// (WBP 里 Visibility 须为 Visible, Hit Test Invisible 不接收点击)
	if (Image_Close)
	{
		Image_Close->OnMouseButtonDownEvent.BindDynamic(this, &UTapComplianceHealthAuthRequiredWidget::OnImageClosePressed);
	}
}

void UTapComplianceHealthAuthRequiredWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 按钮事件
// =============================================================================

FEventReply UTapComplianceHealthAuthRequiredWidget::OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[HealthAuthRequired] Image_Close pressed"));
	OnClosed.ExecuteIfBound();
	RemoveFromParent();

	FEventReply Reply;
	Reply.NativeReply = FReply::Handled();
	return Reply;
}

void UTapComplianceHealthAuthRequiredWidget::OnSwitchAccountClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[HealthAuthRequired] SwitchAccount clicked"));
	// 不在此处销毁面板, 由上层决定 (注销账号后重新走登录流程会有自己的 UI 流转)
	OnSwitchAccount.ExecuteIfBound();
}

void UTapComplianceHealthAuthRequiredWidget::OnExitGameClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[HealthAuthRequired] ExitGame clicked"));
	// 不在此处销毁面板, 由上层调用 KismetSystemLibrary::QuitGame 等方式退出
	OnExitGame.ExecuteIfBound();
}

void UTapComplianceHealthAuthRequiredWidget::OnGoAuthClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[HealthAuthRequired] GoAuth clicked -> notify"));
	// 不在此处销毁面板, 上层跳 RealNamePanel 时 PanelManager.ShowPanel 内部
	// CloseAllPanels 会顺带销毁本面板
	OnGoAuth.ExecuteIfBound();
}

#else // 非桌面平台: 提供空 stub 以满足 UHT 生成代码链接

UTapComplianceHealthAuthRequiredWidget::UTapComplianceHealthAuthRequiredWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceHealthAuthRequiredWidget::Init(
	FOnHealthAuthRequiredSwitchAccount,
	FOnHealthAuthRequiredExitGame,
	FOnHealthAuthRequiredGoAuth,
	FOnHealthAuthRequiredClosed,
	bool) {}

void UTapComplianceHealthAuthRequiredWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceHealthAuthRequiredWidget::NativeDestruct()  { Super::NativeDestruct();  }

FEventReply UTapComplianceHealthAuthRequiredWidget::OnImageClosePressed(FGeometry, const FPointerEvent&) { return {}; }
void UTapComplianceHealthAuthRequiredWidget::OnSwitchAccountClicked() {}
void UTapComplianceHealthAuthRequiredWidget::OnExitGameClicked()      {}
void UTapComplianceHealthAuthRequiredWidget::OnGoAuthClicked()        {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

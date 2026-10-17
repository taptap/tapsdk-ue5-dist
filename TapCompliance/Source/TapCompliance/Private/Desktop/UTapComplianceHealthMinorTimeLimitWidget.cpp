// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceHealthMinorTimeLimitWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"

// UMG
#include "Components/Button.h"
#include "Components/TextBlock.h"

UTapComplianceHealthMinorTimeLimitWidget::UTapComplianceHealthMinorTimeLimitWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// 公共接口
// =============================================================================

void UTapComplianceHealthMinorTimeLimitWidget::Init(
	FOnHealthMinorTimeLimitExitGame      InOnExitGame,
	FOnHealthMinorTimeLimitSwitchAccount InOnSwitchAccount,
	FOnHealthMinorTimeLimitClosed        InOnClosed)
{
	OnExitGame      = InOnExitGame;
	OnSwitchAccount = InOnSwitchAccount;
	OnClosed        = InOnClosed;
}

void UTapComplianceHealthMinorTimeLimitWidget::SetSwitchAccountVisible(bool bVisible)
{
	if (!Button_SwitchAccount)
	{
		// WBP 还没添加 Button_SwitchAccount 控件 → 安全 no-op
		return;
	}
	Button_SwitchAccount->SetVisibility(bVisible
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
}

void UTapComplianceHealthMinorTimeLimitWidget::SetTitle(const FText& NewText)
{
	// 空文案 → 保留 WBP 默认 ("游戏健康提醒"), 不调 SetText.
	// 极端情况下 heartbeat 和 UserAntiConfig.LocalConfig 同时为空时, WBP 默认顶上.
	if (NewText.IsEmpty()) return;
	if (TextBlock_Title)
	{
		TextBlock_Title->SetText(NewText);
	}
}

void UTapComplianceHealthMinorTimeLimitWidget::SetContent(const FText& NewText)
{
	// 同 SetTitle: 空文案保留 WBP 默认法规通用文案兜底.
	if (NewText.IsEmpty()) return;
	if (TextBlock_Body)
	{
		TextBlock_Body->SetText(NewText);
	}
}

void UTapComplianceHealthMinorTimeLimitWidget::SetExitButtonText(const FText& NewText)
{
	if (TextBlock_ExitLabel)
	{
		TextBlock_ExitLabel->SetText(NewText);
		return;
	}
	// Fallback: 万一 WBP 里 TextBlock 名字对不上 BindWidgetOptional,
	// 直接遍历 Button 的子控件改第一个 UTextBlock
	if (Button_ExitGame)
	{
		const int32 N = Button_ExitGame->GetChildrenCount();
		for (int32 i = 0; i < N; ++i)
		{
			if (UTextBlock* TB = Cast<UTextBlock>(Button_ExitGame->GetChildAt(i)))
			{
				TB->SetText(NewText);
				return;
			}
		}
	}
	UE_LOG(LogTapCompliance, Warning,
		TEXT("[HealthMinorTimeLimit] SetExitButtonText: no TextBlock found, ignored"));
}

// =============================================================================
// NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceHealthMinorTimeLimitWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_ExitGame)
	{
		Button_ExitGame->OnClicked.AddDynamic(this, &UTapComplianceHealthMinorTimeLimitWidget::OnExitGameClicked);
	}
	if (Button_SwitchAccount)
	{
		Button_SwitchAccount->OnClicked.AddDynamic(this, &UTapComplianceHealthMinorTimeLimitWidget::OnSwitchAccountClicked);
		// 默认折叠, 由 PanelManager 在禁玩 + showSwitchAccount=true 时调 SetSwitchAccountVisible(true) 打开
		Button_SwitchAccount->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTapComplianceHealthMinorTimeLimitWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 按钮事件
// =============================================================================

void UTapComplianceHealthMinorTimeLimitWidget::OnExitGameClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[HealthMinorTimeLimit] ExitGame clicked"));
	// 与 Unity TaptapComplianceHealthReminderController.OnOKButtonClicked 对齐:
	// Widget 自身职责: 先关面板再 Invoke. 上层 lambda 决定后续动作 (可玩态→LOGIN_SUCCESS+心跳; 禁玩态→QuitGame).
	RemoveFromParent();
	OnExitGame.ExecuteIfBound();
}

void UTapComplianceHealthMinorTimeLimitWidget::OnSwitchAccountClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[HealthMinorTimeLimit] SwitchAccount clicked"));
	// 与 Unity TaptapComplianceHealthReminderController.OnSwitchAccountButtonClicked 对齐:
	// 先关面板再 Invoke. 上层调 Logout(false) + DispatchCallback(SWITCH_ACCOUNT).
	RemoveFromParent();
	OnSwitchAccount.ExecuteIfBound();
}

#else // 非桌面平台: 提供空 stub 以满足 UHT 生成代码链接

UTapComplianceHealthMinorTimeLimitWidget::UTapComplianceHealthMinorTimeLimitWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceHealthMinorTimeLimitWidget::Init(
	FOnHealthMinorTimeLimitExitGame,
	FOnHealthMinorTimeLimitSwitchAccount,
	FOnHealthMinorTimeLimitClosed) {}

void UTapComplianceHealthMinorTimeLimitWidget::SetExitButtonText(const FText&)         {}
void UTapComplianceHealthMinorTimeLimitWidget::SetSwitchAccountVisible(bool)           {}
void UTapComplianceHealthMinorTimeLimitWidget::SetTitle(const FText&)                  {}
void UTapComplianceHealthMinorTimeLimitWidget::SetContent(const FText&)                {}

void UTapComplianceHealthMinorTimeLimitWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceHealthMinorTimeLimitWidget::NativeDestruct()  { Super::NativeDestruct();  }

void UTapComplianceHealthMinorTimeLimitWidget::OnExitGameClicked()      {}
void UTapComplianceHealthMinorTimeLimitWidget::OnSwitchAccountClicked() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

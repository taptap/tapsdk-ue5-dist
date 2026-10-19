// Copyright TapTap. All Rights Reserved.

#include "UTapComplianceHealthMinorPaymentLimitWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"

// UMG
#include "Components/Button.h"
#include "Components/TextBlock.h"

UTapComplianceHealthMinorPaymentLimitWidget::UTapComplianceHealthMinorPaymentLimitWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// =============================================================================
// 公共接口
// =============================================================================

void UTapComplianceHealthMinorPaymentLimitWidget::Init(
	FOnHealthMinorPaymentLimitReturnGame InOnReturnGame,
	FOnHealthMinorPaymentLimitClosed     InOnClosed)
{
	OnReturnGame = InOnReturnGame;
	OnClosed     = InOnClosed;
}

void UTapComplianceHealthMinorPaymentLimitWidget::SetTitle(const FText& NewText)
{
	// 空文案 → 保留 WBP 默认 ("游戏健康提示"), 不调 SetText.
	if (NewText.IsEmpty()) return;
	if (TextBlock_Title)
	{
		TextBlock_Title->SetText(NewText);
	}
}

void UTapComplianceHealthMinorPaymentLimitWidget::SetContent(const FText& NewText)
{
	// 空文案 → 保留 WBP 默认法规通用文案兜底, 不调 SetText.
	// description_plain 为纯文本, UTextBlock::SetText 直接展示 (换行符照常渲染).
	if (NewText.IsEmpty()) return;
	if (TextBlock_Body)
	{
		TextBlock_Body->SetText(NewText);
	}
}

// =============================================================================
// NativeConstruct / NativeDestruct
// =============================================================================

void UTapComplianceHealthMinorPaymentLimitWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Button_ReturnGame)
	{
		Button_ReturnGame->OnClicked.AddDynamic(this, &UTapComplianceHealthMinorPaymentLimitWidget::OnReturnGameClicked);
	}
}

void UTapComplianceHealthMinorPaymentLimitWidget::NativeDestruct()
{
	Super::NativeDestruct();
}

// =============================================================================
// 按钮事件
// =============================================================================

void UTapComplianceHealthMinorPaymentLimitWidget::OnReturnGameClicked()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[HealthMinorPaymentLimit] ReturnGame clicked -> dismiss"));
	// 对齐 Android HealthReminderDialog.kt:78 (TYPE_PAY_LIMIT -> dismissAllowingStateLoss):
	// 支付限额提示只是告知, "返回游戏" 即关闭弹窗、游戏继续, 不退出 / 不切账号 / 不发业务码.
	// 该笔支付被拒已通过 CheckPayable 的 OnDone(allow=false) 回给游戏方, widget 不再触发任何业务码.
	OnReturnGame.ExecuteIfBound();
	RemoveFromParent();
}

#else // 非桌面平台: 提供空 stub 以满足 UHT 生成代码链接

UTapComplianceHealthMinorPaymentLimitWidget::UTapComplianceHealthMinorPaymentLimitWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapComplianceHealthMinorPaymentLimitWidget::Init(
	FOnHealthMinorPaymentLimitReturnGame,
	FOnHealthMinorPaymentLimitClosed) {}

void UTapComplianceHealthMinorPaymentLimitWidget::SetTitle(const FText&)   {}
void UTapComplianceHealthMinorPaymentLimitWidget::SetContent(const FText&) {}

void UTapComplianceHealthMinorPaymentLimitWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapComplianceHealthMinorPaymentLimitWidget::NativeDestruct()  { Super::NativeDestruct();  }

void UTapComplianceHealthMinorPaymentLimitWidget::OnReturnGameClicked() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UTapComplianceHealthMinorPaymentLimitWidget.generated.h"

class UButton;
class UTextBlock;

// 对齐 Android HealthReminderDialog.kt:78 (TYPE_PAY_LIMIT -> dismiss):
// 支付限额提示只是告知, 用户点 "返回游戏" 即关闭弹窗、游戏继续, 不退出游戏 / 不切账号 / 不发业务码.
DECLARE_DELEGATE(FOnHealthMinorPaymentLimitReturnGame); // 点击 "返回游戏" (widget 内部自动 dismiss, 此回调供游戏方做附加处理)
DECLARE_DELEGATE(FOnHealthMinorPaymentLimitClosed);     // 面板关闭兜底 (WBP 已移除右上 X, 现仅 PanelManager 在创建失败时触发)

/**
 * UMG「游戏健康提醒 - 未成年支付限额」对话框
 *
 * 对应资产: /TapCompliance/UI/TapComplianceHealthMinorPaymentLimitPanel
 *
 * 业务定位:
 *   未成年账号当月累计交易达到上限后, 仍尝试支付时强制弹出此提醒, 唯一出口是退出游戏.
 *   依据《关于防止未成年人沉迷网络游戏的通知》:
 *     - 未满 8 周岁: 不提供付费服务
 *     - 8-16 周岁:   单笔不超过 50 元, 月累积不超过 200 元
 *     - 16-18 周岁:  单笔不超过 100 元, 月累积不超过 400 元
 *
 * 控件结构 (与 BindWidget 字段一一对应):
 *   [Root] CanvasPanel
 *   └─ SizeBox_Dialog                  固定 600x320 居中
 *      └─ Border_DialogBg               圆角 8 白底 + 阴影
 *         └─ CanvasPanel_Content
 *            ├─ TitleArea (h=48)
 *            │  └─ TextBlock_Title         "游戏健康提示" (动态 payable.title)          ⭐ BindWidgetOptional
 *            ├─ TextBlock_Body             多行说明 (动态 payable.description_plain)   ⭐ BindWidgetOptional
 *            └─ Button_ReturnGame          居中 220x36, "返回游戏"                     ⭐ BindWidget
 *               └─ TextBlock_ReturnLabel
 *
 * 平台:
 *   仅 Desktop (Win/Mac/Linux) 真正实现, 移动端空 stub.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceHealthMinorPaymentLimitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceHealthMinorPaymentLimitWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 初始化外部回调 (在 AddToViewport 之前调用).
	 * "返回游戏" 由 widget 内部自动 dismiss (RemoveFromParent), 委托仅供游戏方做附加处理 (例如关闭支付 UI), 默认空 lambda 即可.
	 * InOnClosed 现仅作面板关闭兜底 (WBP 已移除右上 X), 由 PanelManager 在创建失败时触发.
	 */
	void Init(
		FOnHealthMinorPaymentLimitReturnGame InOnReturnGame,
		FOnHealthMinorPaymentLimitClosed     InOnClosed);

	/**
	 * 动态设置标题文案 (服务端 payable.title).
	 * 接口 /anti-addiction/v1/payable 返回的 title 为纯文本, 直接 SetText 到 WBP 的 TextBlock_Title.
	 * 空 FText 不调 SetText, 保留 WBP 默认 "游戏健康提示" 作最后兜底.
	 * 对齐 UTapComplianceHealthMinorTimeLimitWidget::SetTitle 与 Android/Unity 同源 title 字段.
	 * WBP 缺少 TextBlock_Title 控件时安全 no-op (BindWidgetOptional).
	 */
	void SetTitle(const FText& NewText);

	/**
	 * 动态设置正文文案 (服务端 payable.description_plain 或本地兜底).
	 * 接口 /anti-addiction/v1/payable 返回的 description_plain 为纯文本 (服务端已渲染数值),
	 * 这里直接 SetText 到 WBP 的 TextBlock_Body (UTextBlock).
	 * 空 FText 不调 SetText, 保留 WBP 默认法规通用文案兜底.
	 * 对齐 UTapComplianceHealthMinorTimeLimitWidget::SetContent 与 Android/Unity 同源字段.
	 * WBP 缺少 TextBlock_Body 控件时安全 no-op (BindWidgetOptional).
	 */
	void SetContent(const FText& NewText);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION 在所有平台都需声明 (UHT 生成代码引用), 不可放入 #if 块内
	UFUNCTION() void OnReturnGameClicked();

private:
	// ============== BindWidget (仅声明 C++ 实际访问的控件) ==============
	UPROPERTY(meta = (BindWidgetOptional)) UButton*    Button_ReturnGame = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Title   = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Body    = nullptr;

	// ============== Runtime state ==============
	FOnHealthMinorPaymentLimitReturnGame OnReturnGame;
	FOnHealthMinorPaymentLimitClosed     OnClosed;
};

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UTapComplianceHealthMinorTimeLimitWidget.generated.h"

class UButton;
class UTextBlock;

// 委托声明
DECLARE_DELEGATE(FOnHealthMinorTimeLimitExitGame);       // 点击 "退出游戏" / "进入游戏" (按 RemainTime 切换语义)
DECLARE_DELEGATE(FOnHealthMinorTimeLimitSwitchAccount);  // 点击 "切换账号" (仅 bShowSwitchAccount=true 且禁玩态)
DECLARE_DELEGATE(FOnHealthMinorTimeLimitClosed);         // 面板关闭兜底 (WBP 已移除右上 X, 现仅 PanelManager 在创建失败时触发)

/**
 * UMG「游戏健康提醒 - 未成年禁玩时段 / 时段允许提醒」对话框
 *
 * 对应资产: /TapCompliance/UI/TapComplianceHealthMinorTimeLimitPanel
 *
 * 业务定位:
 *   未成年账号:
 *     - 在允许时段且 RemainTime > 0 → 弹此提醒, 主按钮文案动态改为 "进入游戏",
 *       用户点击后才进入游戏 (LOGIN_SUCCESS).
 *     - 在禁玩时段 / 时长耗尽 → 弹此提醒, 主按钮 "退出游戏";
 *       若 showSwitchAccount=true 则额外显示 "切换账号" 按钮 (出口 SWITCH_ACCOUNT(1001)).
 *   (允许时段: 周五 / 周六 / 周日 / 法定节假日 20:00-21:00)
 *
 * 文案 (全部 WBP 蓝图写死):
 *   "您当前为 [未成年账号], 已被纳入防沉迷系统。根据国家新闻出版署
 *    《关于防止未成年人沉迷网络游戏的通知》《关于进一步严格管理切实
 *    防止未成年人沉迷网络游戏的通知》, 网络游戏仅可在周五、周六、周日
 *    及法定节假日 20 点 - 21 点向未成年人提供 60 分钟网络游戏服务。
 *    当前时间段 [无法玩游戏], 请合理安排时间。"
 *   - "未成年账号" / "无法玩游戏" 高亮: Regular 14 + #FF8156 橙
 *   - 其余: Regular 14 + #52575D 灰
 *
 * 控件结构 (与 BindWidget 字段一一对应):
 *   [Root] CanvasPanel
 *   └─ SizeBox_Dialog                  固定 600x347 居中
 *      └─ Border_DialogBg               圆角 8 白底 + 阴影
 *         └─ CanvasPanel_Content
 *            ├─ TitleArea (h=48)
 *            │  └─ TextBlock_Title         "游戏健康提醒"
 *            ├─ TextBlock_Body              多行混色说明 (居中, wrap)
 *            ├─ Button_SwitchAccount       (可选, "切换账号" + 16x16 刷新图标, Bold 14 #15C5CE) ⭐ BindWidgetOptional
 *            │                               仅在 showSwitchAccount=true 且禁玩态时由 C++ 显示;
 *            │                               样式参照 UTapComplianceHealthAuthRequiredPanel 的 Button_SwitchAccount
 *            └─ Button_ExitGame            (居中 220x36, y=271, "退出游戏" / "进入游戏") ⭐ BindWidget
 *               └─ TextBlock_ExitLabel     文案动态: 可玩态 "进入游戏" / 禁玩态 "退出游戏"
 *
 * 平台:
 *   仅 Desktop (Win/Mac/Linux) 真正实现, 移动端空 stub.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceHealthMinorTimeLimitWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceHealthMinorTimeLimitWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 初始化外部回调 (在 AddToViewport 之前调用).
	 * 主按钮 / 切换账号 都会自动 RemoveFromParent 销毁面板, 再触发对应回调.
	 * (与 Unity TaptapComplianceHealthReminderController 一致: Close() 后 Invoke())
	 * 上层 lambda 决定后续动作:
	 *   - 可玩态主按钮 → LOGIN_SUCCESS(500) + 启心跳
	 *   - 禁玩态主按钮 → SDK 主动 KismetSystemLibrary::QuitGame (与 Unity Application.Quit 对齐)
	 *   - SwitchAccount → SDK Logout(false) + 回调 SWITCH_ACCOUNT(1001)
	 *
	 * @param InOnExitGame      主按钮 (退出游戏 / 进入游戏)
	 * @param InOnSwitchAccount 可选, 不绑定时按钮自动隐藏
	 * @param InOnClosed        面板关闭兜底 (WBP 已移除右上 X, 现仅 PanelManager 在创建失败时触发)
	 */
	void Init(
		FOnHealthMinorTimeLimitExitGame      InOnExitGame,
		FOnHealthMinorTimeLimitSwitchAccount InOnSwitchAccount,
		FOnHealthMinorTimeLimitClosed        InOnClosed);

	/**
	 * 动态修改主按钮文案
	 *   - RemainTime > 0 (可玩态) 调用方传 "进入游戏"
	 *   - RemainTime <= 0 (禁玩态) WBP 默认 / 调用方传 "退出游戏"
	 *
	 * 实现策略:
	 *   1) 优先 set BindWidgetOptional 的 TextBlock_ExitLabel (WBP 内已存在该控件)
	 *   2) 兜底: 遍历 Button_ExitGame 子控件，对第一个 UTextBlock 调 SetText
	 *
	 * 必须在 NativeConstruct 之后调用 (常见做法是 AddToViewport 后立即调)。
	 */
	void SetExitButtonText(const FText& NewText);

	/**
	 * 设置 "切换账号" 按钮可见性 (默认隐藏)。
	 * 由 PanelManager / AuthFlow 在 bShowSwitchAccount && RemainTime<=0 时设为 true。
	 * WBP 缺少 Button_SwitchAccount 控件时安全 no-op。
	 */
	void SetSwitchAccountVisible(bool bVisible);

	/**
	 * 动态设置标题文案 (服务端 heartbeat.title 或本地兜底 UITipText.{allow|reject}.title).
	 * 空 FText 不调 SetText, 保留 WBP 默认 "游戏健康提醒" 作最后兜底.
	 * 对齐 Android HealthReminderDialog.tvTitle.text = title (HealthReminderDialog.kt:104).
	 * WBP 缺少 TextBlock_Title 控件时安全 no-op.
	 */
	void SetTitle(const FText& NewText);

	/**
	 * 动态设置正文文案 (服务端 heartbeat.description_plain 或本地兜底 UITipText.{allow|reject}.description_plain).
	 * 调用方 (FComplianceAuthFlow::ResolveHealthReminderText) 已处理:
	 *   - 服务端文案为空时回退本地 UITipText
	 *   - 占位符 "# ${remaining} #" 替换 (分钟 / 秒)
	 *   - "分钟→秒" substring 切分 (避免误伤前缀里的 "60 分钟" 字样)
	 * 空 FText 不调 SetText, 保留 WBP 默认法规通用文案兜底.
	 * 使用 description_plain (纯文本, 服务端已渲染数值) → 普通 UTextBlock 即可,
	 * 不需要 RichTextBlock 的 HTML/富文本解析能力.
	 * 对齐 Unity contentText.text = content; Android description_plain 字段同源.
	 * WBP 缺少 TextBlock_Body 控件时安全 no-op.
	 */
	void SetContent(const FText& NewText);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION 在所有平台都需声明 (UHT 生成代码引用), 不可放入 #if 块内
	UFUNCTION() void OnExitGameClicked();
	UFUNCTION() void OnSwitchAccountClicked();

private:
	// ============== BindWidget (仅声明 C++ 实际访问的控件) ==============
	UPROPERTY(meta = (BindWidgetOptional)) UButton*        Button_ExitGame      = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton*        Button_SwitchAccount = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*     TextBlock_ExitLabel  = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*     TextBlock_Title      = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*     TextBlock_Body        = nullptr;

	// ============== Runtime state ==============
	FOnHealthMinorTimeLimitExitGame      OnExitGame;
	FOnHealthMinorTimeLimitSwitchAccount OnSwitchAccount;
	FOnHealthMinorTimeLimitClosed        OnClosed;
};

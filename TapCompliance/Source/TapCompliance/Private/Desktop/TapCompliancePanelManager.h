// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "UTapComplianceRealNameWidget.h"
#include "UTapComplianceFastAuthWidget.h"
#include "UTapComplianceAuthConfirmWidget.h"
#include "UTapComplianceAuthFailedWidget.h"
#include "UTapComplianceAuthReviewingWidget.h"
#include "UTapComplianceHealthAuthRequiredWidget.h"
#include "UTapComplianceHealthMinorTimeLimitWidget.h"
#include "UTapComplianceHealthMinorPaymentLimitWidget.h"

class UTapComplianceRealNameWidget;
class UTapComplianceFastAuthWidget;
class UTapComplianceAuthConfirmWidget;
class UTapComplianceAuthFailedWidget;
class UTapComplianceAuthReviewingWidget;
class UTapComplianceHealthAuthRequiredWidget;
class UTapComplianceHealthMinorTimeLimitWidget;
class UTapComplianceHealthMinorPaymentLimitWidget;

/**
 * 实名认证面板生命周期管理
 * 负责创建/销毁 RealName / FastAuth / AuthConfirm / AuthFailed /
 * HealthAuthRequired / HealthMinorTimeLimit 面板, 管理 Viewport 叠加
 */
class FTapCompliancePanelManager
{
public:
	static FTapCompliancePanelManager& Get();

	/**
	 * 显示实名认证面板
	 * @param OnClose 面板关闭回调 (点击关闭按钮)
	 * @param OnBack 返回回调 (点击返回按钮)
	 * @param OnSubmit 提交回调 (姓名, 身份证号)
	 * @param bHasAuthRecord 对齐 Android RealNameVerifyDialog.newInstance(hasAuthRecord):
	 *                       true 时面板用 ManualAuthFailedTip (auth_failed) 文案 (上次提交未通过,
	 *                       提示重新填写); 默认 false 用 ManualAuthTip (manual_auth) 首次实名文案.
	 */
	void ShowPanel(
		FOnCompliancePanelClose OnClose,
		FOnCompliancePanelBack OnBack,
		FOnCompliancePanelSubmit OnSubmit,
		bool bHasAuthRecord = false);

	/**
	 * 显示「TapTap 快速认证服务」对话框.
	 * 正文 / 标题 / 按钮文案运行时从 FComplianceState.RealNameConfig.RealNameText.TapAuthTip 注入,
	 * 服务端无值时保留 WBP 默认占位 (不再做本地 GameName 模板兜底).
	 *
	 * @param bShowCancelButton   调用方按 Android/Unity 语义传入最终显示策略：
	 *                            manual_auth_enable=false 或 Tap 已登录但缺 compliance scope 时隐藏"不使用".
	 * @param OnConfirmed  点击"去使用"回调
	 * @param OnCancelled  点击"不使用"回调 (按钮隐藏时永不触发)
	 * @param OnClosed     点击关闭按钮回调
	 */
	void ShowFastAuthPanel(
		bool                 bShowCancelButton,
		FOnFastAuthConfirmed OnConfirmed,
		FOnFastAuthCancelled OnCancelled,
		FOnFastAuthClosed    OnClosed);

	/**
	 * 显示「游戏实名认证 - 二次确认」对话框
	 * 用于 FastAuth 流程中, 服务端返回脱敏实名信息后, 让用户确认并同意授权.
	 *
	 * @param MaskedName      脱敏姓名, 如 "张*"
	 * @param MaskedIDNumber  脱敏身份证号, 如 "3101***************"
	 * @param OnConfirmed     点击"同意授权"回调
	 * @param OnClosed        点击关闭按钮回调
	 */
	void ShowAuthConfirmPanel(
		const FString& MaskedName,
		const FString& MaskedIDNumber,
		FOnAuthConfirmConfirmed OnConfirmed,
		FOnAuthConfirmClosed OnClosed);

	/**
	 * 显示「游戏实名认证 - 审核中 / 认证失败」状态对话框 (Waiting + Failed 双态复用).
	 *
	 * 对齐 IdentityVerifyState.kt 三态映射（pass 不进此 panel，由统一 Toast 处理）：
	 *   - State=Waiting → 显示 AuthWaitingTip (auth_waiting)
	 *                     典型调用方: 服务端返回 status="waiting"、用户实名信息已提交
	 *                     仍在审核, OnConfirmed 通常 Finish(REAL_NAME_STOP).
	 *   - State=Failed  → 显示 ManualAuthFailedTip (auth_failed)
	 *                     典型调用方: 手动提交后服务端返回非 verified, OnConfirmed 通常
	 *                     调用 ShowRealNameManual 让用户重填.
	 *
	 * @param State        Waiting / Failed
	 * @param OnConfirmed  点击底部主按钮回调 (widget 内部自动 RemoveFromParent, 本回调仅业务通知)
	 * @param OnClosed     点击右上 X 关闭回调
	 */
	void ShowAuthReviewingPanel(
		EAuthReviewingState State,
		FOnAuthReviewingConfirmed OnConfirmed,
		FOnAuthReviewingClosed OnClosed);

	/**
	 * 显示「TapTap 未实名 - 引导前往游戏内实名」对话框
	 * 用于 FastAuth 流程中, 服务端检测到用户在 TapTap 端未通过国家实名认证
	 * 时调用; 通常 OnConfirmed 回调里跳到 ShowPanel (RealName 手动实名).
	 *
	 * @param OnConfirmed  点击"前往游戏内实名"回调
	 * @param OnClosed     点击关闭按钮回调
	 */
	void ShowAuthFailedPanel(
		FOnAuthFailedConfirmed OnConfirmed,
		FOnAuthFailedClosed OnClosed);

	/**
	 * 显示「游戏健康提醒 - 未完成实名认证」对话框
	 * 游戏运行中检测到当前账号未实名时强制弹出, 三个出口:
	 *   - SwitchAccount: 注销账号, 重新登录
	 *   - ExitGame:      退出游戏 (KismetSystemLibrary::QuitGame)
	 *   - GoAuth:        跳转手动实名 (通常调用 ShowPanel)
	 *
	 * @param bShowSwitchAccount 对齐 Android HealthReminderDialog (HealthReminderDialog.kt:84):
	 *                           =false 时切换账号按钮会被隐藏. 调用方应传入 FComplianceState::bShowSwitchAccount.
	 * @param OnSwitchAccount    点击"切换账号"回调 (按钮隐藏时永不触发)
	 * @param OnExitGame         点击"退出游戏"回调
	 * @param OnGoAuth           点击"去实名"回调
	 * @param OnClosed           点击关闭按钮回调
	 */
	void ShowHealthAuthRequiredPanel(
		bool                               bShowSwitchAccount,
		FOnHealthAuthRequiredSwitchAccount OnSwitchAccount,
		FOnHealthAuthRequiredExitGame      OnExitGame,
		FOnHealthAuthRequiredGoAuth        OnGoAuth,
		FOnHealthAuthRequiredClosed        OnClosed);

	/**
	 * 显示「游戏健康提醒 - 未成年时段」对话框
	 *
	 * @param bAllowEnterGame
	 *   true  = 当前在可玩窗口 (RemainTime > 0)，主按钮文案动态切换为 "进入游戏"，
	 *           OnExitGame 回调实际语义是 "用户确认进入游戏"。
	 *   false = 当前禁玩 (RemainTime <= 0 或非节假日时段)，主按钮保持 "退出游戏"，
	 *           OnExitGame 由游戏侧执行真正的退出流程。
	 *
	 * @param Title           标题文案 (服务端 heartbeat.title 或本地兜底). 空 FText 时
	 *                        Widget 保留 WBP 默认 "游戏健康提醒".
	 * @param Content         正文文案 (服务端 heartbeat.description_plain 或本地兜底, 已
	 *                        由 FComplianceAuthFlow::ResolveHealthReminderText 处理占位符
	 *                        替换). 空 FText 时 Widget 保留 WBP 默认法规通用文案.
	 * @param OnExitGame      主按钮点击回调 (语义随 bAllowEnterGame 切换)
	 * @param OnSwitchAccount "切换账号" 按钮点击回调:
	 *                        - 绑定时按钮可见 (调用方应在 bShowSwitchAccount && !bAllowEnterGame 时绑定)
	 *                        - 不绑定时按钮自动隐藏
	 *                        约定行为: Logout(false) + DispatchCallback(SWITCH_ACCOUNT(1001))
	 * @param OnClosed        右上 X 关闭回调
	 */
	void ShowHealthMinorTimeLimitPanel(
		bool                                 bAllowEnterGame,
		const FText&                         Title,
		const FText&                         Content,
		FOnHealthMinorTimeLimitExitGame      OnExitGame,
		FOnHealthMinorTimeLimitSwitchAccount OnSwitchAccount,
		FOnHealthMinorTimeLimitClosed        OnClosed);

	/**
	 * 显示「游戏健康提醒 - 未成年支付限额」对话框
	 * 未成年账号当月累计交易达上限后再次尝试支付时弹出. 对齐 Android/Unity:
	 * 只是告知性弹窗, "返回游戏"按钮关闭弹窗、游戏继续, 不退出 / 不切账号 / 不发业务码.
	 *
	 * @param Title         标题文案 (服务端 payable.title, 空则保留 WBP 默认 "游戏健康提示")
	 * @param Content       正文文案 (服务端 payable.description_plain, 空则保留 WBP 默认)
	 * @param OnReturnGame  点击"返回游戏"回调 (widget 内部已自动 dismiss, 此回调供游戏方做附加处理)
	 * @param OnClosed      面板关闭兜底 (WBP 已移除右上 X, 现仅创建失败时触发)
	 */
	void ShowHealthMinorPaymentLimitPanel(
		const FText&                         Title,
		const FText&                         Content,
		FOnHealthMinorPaymentLimitReturnGame OnReturnGame,
		FOnHealthMinorPaymentLimitClosed     OnClosed);

	/** 关闭所有已打开的面板 (同时只允许一个面板可见, 新增面板无需再加方法) */
	void CloseAllPanels();

	/** 是否有任意面板正在显示 */
	bool IsAnyPanelShowing() const;

	/** 通知 RealName 面板提交失败 */
	void OnSubmitFailed(const FString& ErrorMessage);

	/** 通知 RealName 面板提交成功 */
	void OnSubmitSuccess();

private:
	FTapCompliancePanelManager() = default;

	/** 把新创建的 widget 登记到统一管理列表 */
	void RegisterPanel(class UUserWidget* Widget);

	// Blueprint 类引用 (首次使用时加载)
	TSubclassOf<UTapComplianceRealNameWidget>             RealNameWidgetClass;
	TSubclassOf<UTapComplianceFastAuthWidget>             FastAuthWidgetClass;
	TSubclassOf<UTapComplianceAuthConfirmWidget>          AuthConfirmWidgetClass;
	TSubclassOf<UTapComplianceAuthFailedWidget>           AuthFailedWidgetClass;
	TSubclassOf<UTapComplianceAuthReviewingWidget>        AuthReviewingWidgetClass;
	TSubclassOf<UTapComplianceHealthAuthRequiredWidget>      HealthAuthRequiredWidgetClass;
	TSubclassOf<UTapComplianceHealthMinorTimeLimitWidget>    HealthMinorTimeLimitWidgetClass;
	TSubclassOf<UTapComplianceHealthMinorPaymentLimitWidget> HealthMinorPaymentLimitWidgetClass;

	// RealName 需要 NotifySubmit*, 单独保留类型化弱引用; 其他 widget 仅依赖 ActivePanels
	TWeakObjectPtr<UTapComplianceRealNameWidget> RealNameWidget;

	// 所有已打开的面板, 用于统一关闭/查询; 使用 TWeakObjectPtr 避免阻碍 GC
	TArray<TWeakObjectPtr<class UUserWidget>> ActivePanels;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

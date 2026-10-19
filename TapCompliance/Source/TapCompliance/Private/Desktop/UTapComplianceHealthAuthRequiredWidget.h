// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"  // FEventReply
#include "UTapComplianceHealthAuthRequiredWidget.generated.h"

class UButton;
class UImage;

// 委托声明
DECLARE_DELEGATE(FOnHealthAuthRequiredSwitchAccount);  // 点击 "切换账号"
DECLARE_DELEGATE(FOnHealthAuthRequiredExitGame);       // 点击 "退出游戏"
DECLARE_DELEGATE(FOnHealthAuthRequiredGoAuth);         // 点击 "去实名"
DECLARE_DELEGATE(FOnHealthAuthRequiredClosed);         // 点击右上 X 关闭

/**
 * UMG「游戏健康提醒 - 未完成实名认证」对话框
 *
 * 对应资产: /TapCompliance/UI/TapComplianceHealthAuthRequiredPanel
 *
 * 业务定位:
 *   游戏运行中检测到当前账号未完成国家实名认证 → 强制弹出此提醒.
 *   用户必须从三个出口中选择:
 *     - 切换账号 → 注销当前账号, 重新登录
 *     - 退出游戏 → 退出整个游戏进程
 *     - 去实名   → 跳转手动实名面板 (RealNamePanel)
 *
 * 文案 (全部 WBP 蓝图写死):
 *   主标题:  "您的账号未完成实名认证！"        (Bold 14, #1D2127)
 *   副说明:  "为了符合国家相关规定，不影响您    (Regular 14, #44D1D8 青)
 *           的游戏体验，请尽快完善实名信息。"
 *   切换账号: "切换账号" + 16x16 刷新图标       (Bold 14, #15C5CE)
 *   退出游戏: "退出游戏"                       (Bold 14, #1D2127, 边框 #EBEBEB)
 *   去实名:   "去实名"                         (Bold 14, 白色, 背景 #15C5CE)
 *
 * 控件结构 (与 BindWidget 字段一一对应):
 *   [Root] CanvasPanel
 *   └─ SizeBox_Dialog                  固定 600x347 居中
 *      └─ Border_DialogBg               圆角 8 白底 + 阴影
 *         └─ CanvasPanel_Content
 *            ├─ TitleArea (h=48)
 *            │  ├─ TextBlock_Title         "游戏健康提醒"
 *            │  └─ Image_Close             右上 22x22 关闭   ⭐ BindWidget
 *            ├─ Image_Warning              50x50 黄色警告图标 (y=88, 居中)
 *            ├─ TextBlock_Header           "您的账号未完成实名认证！"
 *            ├─ TextBlock_Description      副说明 (青色)
 *            ├─ Button_SwitchAccount       内嵌 [刷新图标 + "切换账号"] ⭐ BindWidget
 *            ├─ Button_ExitGame  (左 220x36, y=281)            ⭐ BindWidget
 *            └─ Button_GoAuth    (右 220x36, y=281)            ⭐ BindWidget
 *
 * 平台:
 *   仅 Desktop (Win/Mac/Linux) 真正实现, 移动端空 stub.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceHealthAuthRequiredWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceHealthAuthRequiredWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 初始化外部回调 (在 AddToViewport 之前调用).
	 * 关闭按钮会自动销毁面板; 三个业务按钮不在此处销毁面板,
	 * 由上层根据后续流程决定 (例如 GoAuth 跳转 RealNamePanel 时,
	 * PanelManager.ShowPanel 内部 CloseAllPanels 会顺带销毁本面板).
	 *
	 * @param bInShowSwitchAccount  对齐 Android HealthReminderDialog (HealthReminderDialog.kt:84):
	 *                              =false 时切换账号按钮会被隐藏 (SetVisibility=Collapsed).
	 */
	void Init(
		FOnHealthAuthRequiredSwitchAccount InOnSwitchAccount,
		FOnHealthAuthRequiredExitGame      InOnExitGame,
		FOnHealthAuthRequiredGoAuth        InOnGoAuth,
		FOnHealthAuthRequiredClosed        InOnClosed,
		bool                               bInShowSwitchAccount = true);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION 在所有平台都需声明 (UHT 生成代码引用), 不可放入 #if 块内
	UFUNCTION() FEventReply OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() void OnSwitchAccountClicked();
	UFUNCTION() void OnExitGameClicked();
	UFUNCTION() void OnGoAuthClicked();

private:
	// ============== BindWidget (仅声明 C++ 实际访问的控件) ==============
	UPROPERTY(meta = (BindWidgetOptional)) UImage*  Image_Close           = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_SwitchAccount  = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_ExitGame       = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_GoAuth         = nullptr;

	// ============== Runtime state ==============
	FOnHealthAuthRequiredSwitchAccount OnSwitchAccount;
	FOnHealthAuthRequiredExitGame      OnExitGame;
	FOnHealthAuthRequiredGoAuth        OnGoAuth;
	FOnHealthAuthRequiredClosed        OnClosed;
	bool                               bShowSwitchAccount = true;
};

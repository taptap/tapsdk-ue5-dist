// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"  // FEventReply (UFUNCTION 返回值需完整类型)
#include "UTapComplianceAuthFailedWidget.generated.h"

class UButton;
class UImage;

// 委托声明
DECLARE_DELEGATE(FOnAuthFailedConfirmed);   // 点击 "前往游戏内实名"
DECLARE_DELEGATE(FOnAuthFailedClosed);      // 点击右上关闭

/**
 * UMG「游戏实名认证 - TapTap 未实名引导」对话框
 *
 * 对应资产: /TapCompliance/UI/TapComplianceAuthFailedPanel
 *
 * 业务定位:
 *   用户在 FastAuth 流程同意授权后, 服务端检测到该用户在 TapTap 端
 *   未通过国家实名认证 → 弹此面板提示, 引导用户去游戏内手动实名
 *   (跳转到 UTapComplianceRealNameWidget).
 *
 * 文案 (全部在 WBP 蓝图里写死, C++ 不注入任何动态内容):
 *   "检测到你在 TapTap [未通过国家实名认证], 请直接在游戏中进行实名。"
 *   - "未通过国家实名认证" 高亮显示: Bold + #FF8156 (橙)
 *   - 其余: Regular + #52575D (灰)
 *
 * 控件结构 (与 BindWidget 字段一一对应):
 *   [Root] CanvasPanel
 *   └─ SizeBox_Dialog              固定 600x347 居中
 *      └─ Border_DialogBg           圆角 8 白底 + DropShadow + Inner shadow
 *         └─ CanvasPanel_Content
 *            ├─ TitleArea (h=48)
 *            │  ├─ TextBlock_Title   "游戏实名认证" (居中)
 *            │  └─ Image_Close       右上 22x22 关闭按钮  ⭐ BindWidget
 *            ├─ HBox_Body (居中, y=149 单行文字水平居中)
 *            │  ├─ TextBlock_Prefix      "检测到你在 TapTap "  (灰)
 *            │  ├─ TextBlock_Highlight   "未通过国家实名认证"   (Bold 橙 #FF8156)
 *            │  └─ TextBlock_Suffix      "，请直接在游戏中进行实名。" (灰)
 *            └─ Button_Confirm (294x36, y=271)              ⭐ BindWidget
 *               └─ TextBlock_ConfirmLabel  "前往游戏内实名"
 *
 * 平台:
 *   仅 Desktop (Win/Mac/Linux) 真正实现, 移动端空 stub.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceAuthFailedWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceAuthFailedWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 初始化外部回调 (在 AddToViewport 之前调用).
	 * 关闭按钮会自动销毁面板, OnConfirmed/OnClosed 仅作业务通知.
	 */
	void Init(
		FOnAuthFailedConfirmed InOnConfirmed,
		FOnAuthFailedClosed InOnClosed);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION 在所有平台都需声明 (UHT 生成代码引用), 不可放入 #if 块内
	UFUNCTION() FEventReply OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() void OnConfirmClicked();

private:
	// ============== BindWidget (仅声明 C++ 实际访问的控件) ==============
	UPROPERTY(meta = (BindWidgetOptional)) UImage*  Image_Close    = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_Confirm = nullptr;

	// ============== Runtime state ==============
	FOnAuthFailedConfirmed OnConfirmed;
	FOnAuthFailedClosed    OnClosed;
};

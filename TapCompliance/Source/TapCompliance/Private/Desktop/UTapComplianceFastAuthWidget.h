// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"  // FEventReply (作为 UFUNCTION 返回值需完整类型)
#include "UTapComplianceFastAuthWidget.generated.h"

// Forward declarations
class UButton;
class UImage;
class UTextBlock;
class UTapComplianceEditBox;

// 委托声明
DECLARE_DELEGATE(FOnFastAuthConfirmed);   // 点击 "去使用" — 触发 OAuth 升级 + FastAuth 重试
DECLARE_DELEGATE(FOnFastAuthCancelled);   // 点击 "不使用" — 跳手动实名 (按钮隐藏时永不触发)
DECLARE_DELEGATE(FOnFastAuthClosed);      // 点击右上 X — REAL_NAME_STOP(9002)

/**
 * UMG「TapTap 快速认证服务」对话框
 *
 * 对应资产:  /TapCompliance/UI/TapComplianceFastAuthPanel
 *           (TDS 实名/防沉迷服务【设计】 - TapTap 快速认证服务对话框)
 *
 * 控件结构 (与 BindWidget 字段一一对应):
 *   [Root] CanvasPanel
 *   ├─ Image_Mask                   背景遮罩 (吸收点击)
 *   └─ SizeBox_Dialog               固定 600 宽, 居中
 *      └─ Border_DialogBg           圆角白底
 *         └─ VerticalBox
 *            ├─ TitleBar
 *            │   ├─ TextBlock_Title         "TapTap 快速认证"           ⭐ BindWidget
 *            │   │                          ← TapAuthTip.Title (默认 "TapTap 快速认证")
 *            │   ├─ Button_Back  → Image_Back
 *            │   └─ Button_Close → Image_Close
 *            ├─ Content
 *            │   ├─ Image_Icon              T_IconRealName
 *            │   ├─ TextBlock_Description   ← TapAuthTip.Content (description_plain, 服务端已带游戏名)
 *            │   │                            UTextBlock 自动换行, 为空则保留 WBP 设计稿默认正文,
 *            │   │                            C++ 不做本地兜底
 *            │   └─ TextBlock_Question      "是否使用快速认证？"
 *            └─ Buttons
 *                ├─ Button_Cancel → TextBlock_CancelLabel   ⭐ BindWidget
 *                │                  ← TapAuthTip.NegativeButtonText (默认 "不使用")
 *                └─ Button_Confirm → TextBlock_ConfirmLabel ⭐ BindWidget
 *                                   ← TapAuthTip.PositiveButtonText (默认 "使用")
 *
 * 自适应:
 *   依赖项目级 DPI Curve, SizeBox_Dialog 中心锚定, 不需要手动 ScaleBox.
 *
 * 平台:
 *   仅 Desktop (Win/Mac/Linux) 有实际 UMG 实现, 移动平台空实现以满足 UHT 生成代码编译.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceFastAuthWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceFastAuthWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 初始化外部回调 (在 AddToViewport 之前调用).
	 * Confirm/Cancel/Close 均会在对应按钮点击时触发对应委托.
	 *
	 * @param bInShowCancelButton 调用方按 Android/Unity 语义传入:
	 *                            manual_auth_enable=false 或 Tap 已登录但缺 compliance scope 时隐藏"不使用".
	 */
	void Init(
		FOnFastAuthConfirmed InOnConfirmed,
		FOnFastAuthCancelled InOnCancelled,
		FOnFastAuthClosed    InOnClosed,
		bool                 bInShowCancelButton = true);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION 在所有平台都需声明 (UHT 生成代码引用), 不可放入 #if 块内
	// Image_Close 是 UImage, 没有 OnClicked, 走 OnMouseButtonDownEvent (返回 FEventReply)
	UFUNCTION() FEventReply OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() void OnCancelClicked();
	UFUNCTION() void OnConfirmClicked();

	// 输入框文本变化: 用于在空 / 非空之间切换 ForegroundColor
	// (UE5 EditableTextBox 没有独立的 HintColor, hint 与输入文字共用 ForegroundColor)
	UFUNCTION() void OnNameInputTextChanged(const FText& Text);
	UFUNCTION() void OnIDNumberInputTextChanged(const FText& Text);

	/**
	 * 把服务端 TapAuthTip 中非空字段 SetText 到对应控件 (Title / Body / 两个按钮 label).
	 * 空字段不调 SetText, 保留 WBP 默认占位. 必须延后到 NativeConstruct 之后调用.
	 */
	void ApplyServerPromptText();

private:
	// ============== BindWidget (仅声明 C++ 实际访问的控件) ==============
	UPROPERTY(meta = (BindWidgetOptional)) UImage*  Image_Close    = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_Cancel  = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* Button_Confirm = nullptr;

	// 服务端文案注入目标 (运行时按 TapAuthTip 字段非空原则覆盖, 否则保留 WBP 默认占位)
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Title        = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Description  = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_ConfirmLabel = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_CancelLabel  = nullptr;

	// 输入框 (需为 TapComplianceEditBox / NumericEditBox, 才能安全刷 hint/正文色)
	UPROPERTY(meta = (BindWidgetOptional)) UTapComplianceEditBox* NameInput     = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTapComplianceEditBox* IDNumberInput = nullptr;

	// ============== Runtime state (与平台无关) ==============
	FOnFastAuthConfirmed OnConfirmed;
	FOnFastAuthCancelled OnCancelled;
	FOnFastAuthClosed    OnClosed;

	/** false 时 NativeConstruct 中会隐藏 Button_Cancel */
	bool bShowCancelButton = true;
};

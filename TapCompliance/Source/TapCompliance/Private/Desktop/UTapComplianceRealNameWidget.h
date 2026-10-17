// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"  // FEventReply (作为 UFUNCTION 返回值需完整类型)
#include "UTapComplianceRealNameWidget.generated.h"

// Forward declarations
class UButton;
class UImage;
class UTextBlock;
class UTapComplianceEditBox;
class UTapComplianceNumericEditBox;
class UWidget;

// 委托声明
DECLARE_DELEGATE(FOnCompliancePanelClose);
DECLARE_DELEGATE(FOnCompliancePanelBack);
DECLARE_DELEGATE_TwoParams(FOnCompliancePanelSubmit, const FString& /*Name*/, const FString& /*IDNumber*/);

/**
 * UMG 实名认证面板
 * 参照 Figma 设计稿: 游戏实名认证对话框
 * 使用 BindWidget + Blueprint 模式: C++ 管理业务逻辑, Blueprint 管理布局和样式
 *
 * 自适应:
 *   依赖项目级 DPI Curve 与 WBP 固定尺寸 (DialogSizeBox 800x480),
 *   与 FastAuth 一致, C++ 不再手动改 ScaleBox.
 *
 * Blueprint 层级:
 *   [Canvas Panel] Root (全屏)
 *   └── [ScaleBox] DialogScaler      ← WBP Stretch=UserSpecified, 默认 1.0
 *       └── [SizeBox] DialogSizeBox  ← WBP 固定 800x480
 *           └── 弹窗内容
 *               ├── TextBlock_Title          ⭐ BindWidget
 *               │                            ← ManualAuthTip.Title           (bHasAuthRecord=false)
 *               │                            或 ManualAuthFailedTip.Title    (bHasAuthRecord=true)
 *               │                              (默认 "游戏实名认证")
 *               ├── TextBlock_Description    ⭐ BindWidget
 *               │                            ← 同上来源的 .Content (description_plain 纯文本)
 *               │                              (说明文本 / 上次失败时的失败原因)
 *               ├── NameInput / IDNumberInput / ErrorHint / ...
 *               └── SubmitBtn
 *                   └── TextBlock_SubmitLabel ⭐ BindWidget
 *                                             ← 同上来源的 .PositiveButtonText
 *                                               (默认 "提交" / "我知道了")
 *
 * 文案动态注入策略 (运行时优先服务端, 字段空保留 WBP 默认占位):
 *   - bHasAuthRecord=false → RealNameConfig.RealNameText.ManualAuthTip
 *   - bHasAuthRecord=true  → RealNameConfig.RealNameText.ManualAuthFailedTip
 *
 * 注: 仅在 Desktop (Win/Mac/Linux) 平台有实际实现,
 *     移动平台提供空实现以满足 UHT 生成代码编译需求
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceRealNameWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceRealNameWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 初始化面板参数 (在 AddToViewport 之前调用).
	 *
	 * @param bInHasAuthRecord 对齐 Android RealNameVerifyDialog.newInstance(hasAuthRecord):
	 *                         true  → 用户此前提交的实名信息被国家系统判定 failed, 用
	 *                                 ManualAuthFailedTip (auth_failed) 文案要求重新填写;
	 *                         false → 首次进入手动实名, 用 ManualAuthTip (manual_auth) 文案.
	 *                         默认 false. 决定权在调用方 (Flow / PanelManager).
	 */
	void Init(
		FOnCompliancePanelClose InOnClose,
		FOnCompliancePanelBack InOnBack,
		FOnCompliancePanelSubmit InOnSubmit,
		bool bInHasAuthRecord = false);

	/** 外部通知提交失败 (显示错误提示) */
	void NotifySubmitFailed(const FString& ErrorMessage);

	/** 外部通知提交成功 */
	void NotifySubmitSuccess();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION: UMG 回调 (必须在所有平台声明, 不可放在 #if 块内)
	UFUNCTION() void OnSubmitClicked();
	UFUNCTION() void OnNameInputChanged(const FText& Text);
	UFUNCTION() void OnIDNumberInputChanged(const FText& Text);

	// Image_Close 是 UImage, 没有 OnClicked, 走 OnMouseButtonDownEvent (返回 FEventReply)
	UFUNCTION() FEventReply OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent);

	// BindWidgetOptional: Blueprint 中有同名控件时自动绑定，无则为 nullptr
	// 标题栏 (WBP 仅提供 Image_Close 作为关闭入口, 无独立 Close/Back Button)
	UPROPERTY(meta = (BindWidgetOptional)) UImage*  Image_Close = nullptr;

	// 输入框 (NameInput 需为 TapComplianceEditBox, 才能安全刷 hint/正文色)
	UPROPERTY(meta = (BindWidgetOptional)) UTapComplianceEditBox* NameInput = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTapComplianceNumericEditBox* IDNumberInput = nullptr;

	// 提交按钮
	UPROPERTY(meta = (BindWidgetOptional)) UButton* SubmitBtn = nullptr;

	// 错误提示
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* ErrorHintArea = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ErrorHintText = nullptr;

	// 服务端文案注入目标 (NativeConstruct 后按 ManualAuthTip / ManualAuthFailedTip 二选一注入)
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Title       = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Description = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_SubmitLabel = nullptr;

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	/** 显示错误提示 */
	void ShowError(const FString& Message);
	/** 隐藏错误提示 */
	void HideError();
	/** 设置提交按钮启用状态 */
	void UpdateSubmitButtonState();
	FString GetNormalizedNameInput() const;
	FString GetNormalizedIDNumberInput() const;
	bool IsDuplicateSubmittedInfo(const FString& Name, const FString& IDNumber) const;
	/**
	 * 把服务端 ManualAuthTip / ManualAuthFailedTip (据 bHasAuthRecord 二选一) 中非空字段
	 * SetText 到对应 TextBlock; 空字段保留 WBP 默认占位. 必须延后到 NativeConstruct 之后调用.
	 */
	void ApplyServerPromptText();

	// 委托
	FOnCompliancePanelClose OnClose;
	FOnCompliancePanelBack OnBack;
	FOnCompliancePanelSubmit OnSubmit;

	// 状态
	bool bSubmitInProgress = false;
	bool bHasSubmittedInfo = false;
	FString LastSubmittedName;
	FString LastSubmittedIDNumber;
	// true 时用 ManualAuthFailedTip (auth_failed) 文案, false 用 ManualAuthTip (manual_auth).
	bool bHasAuthRecord = false;
#endif
};

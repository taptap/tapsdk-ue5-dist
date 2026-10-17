// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"  // FEventReply (UFUNCTION 返回值需完整类型)
#include "UTapComplianceAuthConfirmWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

// 委托声明 (用户操作回调)
DECLARE_DELEGATE(FOnAuthConfirmConfirmed);   // 点击 "同意授权"
DECLARE_DELEGATE(FOnAuthConfirmClosed);      // 点击右上关闭

/**
 * UMG「游戏实名认证 - 二次确认」对话框
 *
 * 对应资产: /TapCompliance/UI/TapComplianceAuthConfirmPanel
 *
 * 业务定位:
 *   用户在 FastAuth 流程点了 "去使用" 后, 弹出此面板展示
 *   服务端返回的脱敏实名信息 (姓名 + 身份证号), 让用户点
 *   "同意授权" 完成授权流程.
 *
 * 控件结构 (与 BindWidget 字段一一对应):
 *   [Root] CanvasPanel
 *   └─ SizeBox_Dialog              固定 600x347 居中
 *      └─ Border_DialogBg           圆角 8 白底 + DropShadow + Inner shadow
 *         └─ CanvasPanel_Content    内部绝对定位, 与 Figma 坐标对齐
 *            ├─ TitleArea (h=48)
 *            │  ├─ TextBlock_Title         "游戏实名认证" (居中)
 *            │  └─ Image_Close             右上 22x22 关闭按钮
 *            ├─ Body (520x223, y=48)
 *            │  ├─ TextBlock_Description   "你已使用认证服务..."
 *            │  └─ ConfirmList (居中)
 *            │     ├─ TextBlock_Name       "•  真实姓名:   {脱敏姓名}"
 *            │     └─ TextBlock_IDNumber   "•  身份证号:   {脱敏身份证号}"
 *            └─ Button_Confirm (294x36, y=271)
 *               └─ TextBlock_ConfirmLabel  "同意授权"
 *
 * 平台:
 *   仅 Desktop (Win/Mac/Linux) 真正实现, 移动端空 stub.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceAuthConfirmWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceAuthConfirmWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 设置脱敏后的实名信息 (在 AddToViewport 之前或之后任意时机调用).
	 *
	 * @param InMaskedName      脱敏姓名, 如 "张*"
	 * @param InMaskedIDNumber  脱敏身份证号, 如 "3101***************"
	 */
	void SetUserInfo(const FString& InMaskedName, const FString& InMaskedIDNumber);

	/**
	 * 初始化外部回调 (在 AddToViewport 之前调用).
	 * 关闭按钮会自动销毁面板, OnConfirmed/OnClosed 仅作业务通知.
	 */
	void Init(
		FOnAuthConfirmConfirmed InOnConfirmed,
		FOnAuthConfirmClosed InOnClosed);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION 在所有平台都需声明 (UHT 生成代码引用), 不可放入 #if 块内
	// Image_Close 没有 OnClicked, 走 OnMouseButtonDownEvent (返回 FEventReply)
	UFUNCTION() FEventReply OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() void OnConfirmClicked();

	/** 用当前的 MaskedName / MaskedIDNumber 渲染列表文本 */
	void RefreshUserInfoText();

private:
	// ============== BindWidget (仅声明 C++ 实际访问的控件) ==============
	UPROPERTY(meta = (BindWidgetOptional)) UImage*     Image_Close       = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton*    Button_Confirm    = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Name    = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_IDNumber = nullptr;

	// ============== Runtime state (与平台无关) ==============
	FString MaskedName;
	FString MaskedIDNumber;

	/**
	 * 业务侧是否提供了脱敏实名信息.
	 * false → WBP 里 TextBlock_Name / TextBlock_IDNumber 的默认占位文本保留,
	 *         C++ 不调 SetText (避开 NativeConstruct 阶段对 UTextBlock 的副作用,
	 *         否则 GC 析构子 UTextBlock 时会读野指针, 与 FastAuth 同源问题).
	 * true  → 收到业务数据, 延后到首帧之后再 SetText.
	 */
	bool bHasCustomUserInfo = false;

	FOnAuthConfirmConfirmed OnConfirmed;
	FOnAuthConfirmClosed    OnClosed;
};

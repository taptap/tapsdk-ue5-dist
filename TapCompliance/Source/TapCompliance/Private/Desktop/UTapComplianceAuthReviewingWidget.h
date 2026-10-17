// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"  // FEventReply (UFUNCTION 返回值需完整类型)
#include "UTapComplianceAuthReviewingWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;

// 委托声明
DECLARE_DELEGATE(FOnAuthReviewingConfirmed);   // 点击底部主按钮 (默认"我知道了")
DECLARE_DELEGATE(FOnAuthReviewingClosed);      // 点击右上 X 关闭

/**
 * AuthReviewing panel 双态枚举, 对齐 IdentityVerifyState.kt:
 *   Waiting ↔ "waiting" ↔ FCompliancePrompt = RealNameText.AuthWaitingTip   (auth_waiting)
 *   Failed  ↔ "failed"  ↔ FCompliancePrompt = RealNameText.ManualAuthFailedTip (auth_failed)
 *
 * "pass" 不进此 panel，业务侧只显示统一 Toast“提交成功”.
 *
 * 标记 BlueprintType 后 WBP 蓝图可以读 GetState() 返回值切图标 / 颜色 / 按钮文案.
 */
UENUM(BlueprintType)
enum class EAuthReviewingState : uint8
{
	Waiting UMETA(DisplayName = "审核中 (auth_waiting)"),
	Failed  UMETA(DisplayName = "认证失败 (auth_failed)")
};

/**
 * UMG「游戏实名认证 - 审核中 / 失败」状态对话框 (双态复用)
 *
 * 对应资产: /TapCompliance/UI/TapComplianceAuthReviewingPanel
 *
 * 业务定位 (对齐 IdentityVerifyState.kt 三态映射):
 *   - "waiting" → State=Waiting → 读 AuthWaitingTip (auth_waiting)
 *                 告知用户实名信息已提交但仍在审核, 暂时无法体验游戏.
 *                 三个触发点 (ComplianceAuthFlow.cpp): Step1 server waiting / FastAuth
 *                 waiting / 手动提交后 server 返回 waiting.
 *   - "failed"  → State=Failed  → 读 ManualAuthFailedTip (auth_failed)
 *                 告知用户上次提交的实名信息未通过国家系统认证, 需重新填写.
 *                 触发点: 手动提交后 server 返回非 verified.
 *   - "pass"    → 不进此 panel，业务侧显示统一 Toast“提交成功”.
 *
 * 文案 (运行时优先用服务端 prompt 注入, 字段为空时保留 WBP 默认占位; 全部用
 *       description_plain 纯文本, 不解析 HTML/富文本标签):
 *   标题  ← Prompt.Title              (默认 "游戏实名认证")
 *   正文  ← Prompt.Content            (description_plain)
 *   按钮  ← Prompt.PositiveButtonText (默认 "我知道了")
 *
 *   两个状态可在 WBP 侧通过 PreConstruct / 蓝图变量切换图标 / 主色配色, C++ 仅注入文案.
 *
 * 控件结构 (与 BindWidget 字段一一对应, 命名严格匹配以触发自动绑定):
 *   [Root] CanvasPanel
 *   └─ SizeBox_Dialog                固定 600x347 居中
 *      └─ Border_DialogBg             圆角 8, 白底 #FFFFFF
 *         │                            + DropShadow (0,2) blur 5  色 #00000014
 *         │                            + InnerShadow (0,-2)        色 #F2F2F2
 *         └─ CanvasPanel_Content      内部绝对定位, 与 Figma 坐标对齐
 *            ├─ TitleArea (h=48)
 *            │  ├─ TextBlock_Title           "游戏实名认证"             ⭐ BindWidget
 *            │  │                            居中 / Source Han Sans CN Bold 16 / #1D2127 / 行高 24
 *            │  └─ Image_Close               右上 22x22 关闭图标         ⭐ BindWidget
 *            │                               (right=22, top=12)
 *            ├─ Body (520x223, x=40, y=48)
 *            │  └─ TextBlock_Description     正文 (description_plain 纯文本) ⭐ BindWidget
 *            │                               居中, 行高 22, 宽 520, 自动换行
 *            │                               Source Han Sans CN Regular 14 / #52575D
 *            └─ Button_Confirm (294x36, x=153, y=271)                       ⭐ BindWidget
 *               │                  圆角 25 / 背景 #15C5CE (Primary/SDKBlue)
 *               └─ TextBlock_ConfirmLabel    默认 "我知道了"               ⭐ BindWidget
 *                                            Source Han Sans CN Bold 14 / #FFFFFF / 行高 22
 *
 * 设计 token (来自 Figma):
 *   - 主色 Primary/SDKBlue:  #15C5CE
 *   - 中性 6 / 正文灰:        #52575D
 *   - 中性 7 / 标题深:        #1D2127
 *   - 高亮橙:                #FF8156
 *   - 卡片阴影 Shadow_H1-Card:
 *       InnerShadow color=#F2F2F2 offset=(0,-2) radius=0 spread=0
 *       DropShadow  color=#00000014 offset=(0,2)  radius=5 spread=0
 *
 * 平台:
 *   仅 Desktop (Win/Mac/Linux) 真正实现, 移动端空 stub 满足 UHT 链接.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceAuthReviewingWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapComplianceAuthReviewingWidget(const FObjectInitializer& ObjectInitializer);

	/**
	 * 初始化外部回调 (在 AddToViewport 之前调用).
	 * 关闭按钮与底部主按钮都会自动 RemoveFromParent, OnConfirmed/OnClosed
	 * 仅作业务通知 (例如 Waiting 上层 Finish(REAL_NAME_STOP),
	 * Failed 上层 ShowRealNameManual 让用户重填).
	 *
	 * @param InState  Waiting / Failed, 决定注入哪份 prompt 与 WBP 端切到哪个视觉状态.
	 */
	void Init(
		EAuthReviewingState InState,
		FOnAuthReviewingConfirmed InOnConfirmed,
		FOnAuthReviewingClosed InOnClosed);

	/** WBP / 蓝图侧可读, 用于按状态切图标和主色 */
	UFUNCTION(BlueprintCallable, Category="TapCompliance")
	EAuthReviewingState GetState() const { return State; }

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION 在所有平台都需声明 (UHT 生成代码引用), 不可放入 #if 块内
	UFUNCTION() FEventReply OnImageClosePressed(FGeometry MyGeometry, const FPointerEvent& MouseEvent);
	UFUNCTION() void OnConfirmClicked();

	/**
	 * 按 State 选 AuthWaitingTip / ManualAuthFailedTip, 非空字段 SetText 到对应 TextBlock,
	 * 空字段保留 WBP 默认文案. 必须延后到 NativeConstruct 之后调用.
	 */
	void ApplyServerPromptText();

private:
	// ============== BindWidget (仅声明 C++ 实际访问的控件) ==============
	UPROPERTY(meta = (BindWidgetOptional)) UImage*     Image_Close            = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton*    Button_Confirm         = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Title        = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_Description  = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TextBlock_ConfirmLabel = nullptr;

	// ============== Runtime state ==============
	EAuthReviewingState       State = EAuthReviewingState::Waiting;
	FOnAuthReviewingConfirmed OnConfirmed;
	FOnAuthReviewingClosed    OnClosed;
};

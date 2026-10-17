// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UTapComplianceLoadingWidget.generated.h"

/**
 * 全屏 Loading 蒙层 (对齐 Unity UIManager.OpenLoading / CloseLoading)
 *
 * **纯 Slate 实现**, 不依赖 UMG / WBP 蓝图资产:
 *   - 全屏 SBorder + 半透明黑底 (吸收点击, 防止穿透到游戏 HUD)
 *   - 居中 SCircularThrobber (UE 内置, 自动旋转)
 *   - 通过 UGameViewportClient::AddViewportWidgetContent 直接挂到 Viewport
 *
 * 业务定位:
 *   StartUp 期间 GlobalConfig / Verification / UserAntiConfig 几轮网络请求
 *   累计 1-3s, 用 loading 蒙层避免黑屏 / 无反馈.
 *   任何业务面板出现时 (PanelManager.CloseAllPanels) 自动收掉,
 *   Finish() 兜底也收掉.
 *
 * 单例语义: 同时只允许一个实例; 重复 ShowLoading 为 no-op.
 *
 * 平台: 仅 Desktop (Win/Mac/Linux). 移动端空 stub.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceLoadingWidget : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 显示全屏 Loading 蒙层. 重复调用 no-op.
	 * @param WorldContextObject 仅用于推断 UGameViewportClient (通常用 GEngine->GameViewport->GetWorld())
	 */
	UFUNCTION(BlueprintCallable, Category="TapCompliance|UI",
	          meta=(WorldContext="WorldContextObject"))
	static void ShowLoading(UObject* WorldContextObject);

	/** 立即隐藏 Loading 蒙层. 未显示时为 no-op. */
	UFUNCTION(BlueprintCallable, Category="TapCompliance|UI")
	static void HideLoading();

	/** 是否正在显示 */
	UFUNCTION(BlueprintCallable, Category="TapCompliance|UI")
	static bool IsShowing();
};

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UTapComplianceToast.generated.h"

/**
 * 底部居中 黑底白字 通用 Toast (纯 Slate, 不依赖 WBP 蓝图资产)
 *
 * 对齐 Unity TapTapComplianceUI.OpenToast(message, type) 与
 * Android Toast.makeText: 业务侧通用浮层提示, 覆盖成功 / 失败 / 普通提示等所有场景.
 *
 * 当前 Toast 统一使用纯 Slate 实现：
 *   - 不再按语义拆分 WBP 资产
 *   - 文案运行时传入，不需要额外 UI 资源
 *
 * 业务定位:
 *   - 实名/合规面板内嵌错误条 (ErrorHintArea) 在 WBP 中缺失时, 兜底显示失败原因
 *   - 服务端实名审核拒绝 ("认证未通过，请提交真实信息")
 *   - 首次通过防沉迷校验 / 实名提交成功
 *   - 其它任意一次性通知场景
 *
 * 单例语义: 同时只显示一个 toast; 重复 ShowToast 替换旧的并重新计时.
 *
 * 平台: 仅 Desktop (Win/Mac/Linux). 移动端空 stub.
 */
UCLASS()
class TAPCOMPLIANCE_API UTapComplianceToast : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * 显示 Toast (底部居中, 自动消失).
	 *
	 * @param WorldContextObject 任意持有 World 的对象 (用于推断 UGameViewportClient)
	 * @param Message            文案 (动态)
	 * @param DurationSeconds    自动消失时长; <= 0 表示常驻直到 HideToast
	 */
	UFUNCTION(BlueprintCallable, Category="TapCompliance|UI",
	          meta=(WorldContext="WorldContextObject"))
	static void ShowToast(
		UObject* WorldContextObject,
		const FString& Message,
		float DurationSeconds = 2.5f);

	/** 立即隐藏 Toast (无 toast 时 no-op). */
	UFUNCTION(BlueprintCallable, Category="TapCompliance|UI")
	static void HideToast();
};

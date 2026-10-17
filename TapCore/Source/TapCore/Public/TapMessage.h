// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * 游戏内 Toast 消息提示
 * 对齐 Unity TapMessage：底部/顶部弹出文字提示，自动消失
 * 使用 Slate 实现，在 Shipping 构建中也可正常显示
 */
class TAPCORE_API FTapMessage
{
public:
	enum class EPosition : uint8 { Top, Bottom };
	enum class EDuration : uint8 { OneSecond, TwoSecond, ThreeSecond };

	/**
	 * 显示 Toast 消息
	 * @param Msg 显示的文本
	 * @param Position 位置（顶部/底部），默认底部
	 * @param Duration 显示时长，默认 2 秒
	 */
	static void ShowMessage(
		const FString& Msg,
		EPosition Position = EPosition::Bottom,
		EDuration Duration = EDuration::TwoSecond);

private:
	/** 内部实现，支持延迟重试（Viewport 未就绪时等待） */
	static void ShowMessageInternal(
		const FString& Msg,
		EPosition Position,
		EDuration Duration,
		int32 DeferCount);
};

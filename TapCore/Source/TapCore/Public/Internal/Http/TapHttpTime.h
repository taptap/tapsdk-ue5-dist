// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * HTTP 时间管理
 *
 * 对齐 Android TapTime (tap-common/.../http/TapTime.kt):
 *   一旦从服务端响应里拿到 now 校准过, 之后所有 GetCurrentTime() 都基于
 *   单调时钟 (FPlatformTime::Seconds) 计算, 用户改系统时间不会影响 X-Tap-Ts.
 *   未校准前 fallback 用 wall-clock 作初值, 避免冷启动首请求带 0.
 */
class TAPCORE_API FTapHttpTime
{
public:
	/** 当前时间戳 (秒). 已校准则用 server time + monotonic elapsed, 否则用 wall clock. */
	static int64 GetCurrentTime();

	/**
	 * 用服务端响应里的 now 字段校准本地时钟.
	 * @param ServerTime 服务端时间戳 (秒). <=0 忽略.
	 */
	static void FixTime(int64 ServerTime);

	/** 等价于 FixTime, 保留兼容 (历史 API). */
	static void ResetLastServerTime(int64 ServerTime);

	/** 已校准时返回校准后的当前时间 (秒); 未校准返回 0. */
	static int64 GetCurrentServerTime();

	/** 是否已用 server time 校准过 */
	static bool IsTimeSynced();

private:
	/** 校准过否 (对齐 Android isServerTime) */
	static bool bIsServerTime;

	/** server now (ms) - monotonic (ms) at calibration. 对齐 Android timeDiff. */
	static int64 TimeDiffMs;

	/** 取 monotonic 时钟 (毫秒). UE 用 FPlatformTime::Seconds, 对齐 Android SystemClock.elapsedRealtime. */
	static int64 GetMonotonicMs();
};

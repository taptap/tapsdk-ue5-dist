// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Containers/Ticker.h"
#include "ComplianceNetwork.h"

/**
 * Desktop 合规模块：心跳轮询 + 1s 倒计时
 *
 * 启动后维护两个 ticker：
 *   - HeartbeatTicker: 周期 = policy.heartbeat_interval (默认 120s)，
 *     每次发 /anti-addiction/v1/heartbeat，刷新 RemainSeconds 与 PlayableResult。
 *   - CountdownTicker: 1s，递减 RemainSeconds，提供给 GetUserRemainTime 同步查询。
 *
 * 心跳结果分支:
 *   RemainTime > interval     → 等下一轮
 *   1 <= RemainTime <= interval → 缩短下一轮 ticker 周期到 RemainTime+1
 *   RemainTime <= 0           → 停 ticker，发 PERIOD_RESTRICT(1030) 并弹 HealthMinorTimeLimit
 *   token expired             → Logout + EXITED(1000)
 *   其它异常                  → INVALID_CLIENT_OR_NETWORK_ERROR(1200) 并停 ticker
 *
 * 前后台:
 *   FCoreDelegates::ApplicationWillDeactivateDelegate → Pause()
 *   FCoreDelegates::ApplicationHasReactivatedDelegate → Resume() + 立即触发一次心跳
 *
 * 单例 (与流程同生命周期，不需要多个实例)。
 */
class FCompliancePoll
{
public:
	static FCompliancePoll& Get();

	/** 启动轮询；intervalSec <=0 时使用默认 120 */
	void StartUp(int32 IntervalSec);

	/** 停止轮询并清理倒计时 */
	void Logout();

	/** 暂停 (前台失活) */
	void Pause();

	/** 恢复 + 立即触发一次心跳 */
	void Resume();

	/** 是否正在轮询 */
	bool IsRunning() const { return bRunning; }

private:
	friend class FCompliancePollSessionIsolationTest;
	FCompliancePoll();
	~FCompliancePoll();

	FCompliancePoll(const FCompliancePoll&) = delete;
	FCompliancePoll& operator=(const FCompliancePoll&) = delete;

	/** 触发一次心跳 (异步)；完成后根据剩余时间安排下一次 */
	void TickHeartbeatOnce();

	/** Bind a response to the polling lifetime and identity that sent the request. */
	FComplianceNetwork::FOnPlayable CreateHeartbeatCallback(const FString& UserId, const FString& SessionId);

	/** 把 (心跳成功 / 本地计算) 拿到的可玩态写入 state, 并按 RemainTime 决定停 / 续 / 弹窗 */
	void HandlePlayableUpdate(const struct FPlayableResult& Result);

	/** 添加心跳 ticker，按 IntervalSec 周期 */
	void ScheduleHeartbeat(int32 IntervalSec);

	/** 添加 / 重置 1s 倒计时 ticker */
	void EnsureCountdownTicker();

	/** 移除心跳 ticker */
	void StopHeartbeatTicker();

	/** 移除倒计时 ticker */
	void StopCountdownTicker();

	void BindAppLifecycle();
	void UnbindAppLifecycle();

private:
	// Game-thread only, like the lifecycle methods and HTTP completion callbacks.
	uint64 SessionGeneration = 0;
	bool bRunning = false;
	bool bPaused  = false;

	int32 IntervalSec = 120;

	FTSTicker::FDelegateHandle HeartbeatHandle;
	FTSTicker::FDelegateHandle CountdownHandle;

	FDelegateHandle DeactivateHandle;
	FDelegateHandle ReactivateHandle;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

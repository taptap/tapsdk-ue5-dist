// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceModels.h"
#include "HAL/CriticalSection.h"

/**
 * Desktop 合规模块全局状态 (单例, 进程内唯一)
 *
 * 持有：
 *   - 当前会话标识 (UserId / SessionId / Token)
 *   - 实名信息 (Verification)
 *   - 服务端配置 (UserAntiConfig / RealNameConfig)
 *   - 当前可玩态 (PlayableResult + RemainSeconds 倒计时)
 *   - 运行时选项 (bShowSwitchAccount / bUseAgeRange)
 *   - 持久回调注册表 (Compliance Event Callbacks)
 *
 * 线程模型：
 *   - 大部分字段同一时刻只在 Game Thread 上读写，未加锁；
 *   - CallbackList 用 CallbackLock 保护 (回调注册可能来自任意线程)；
 *   - RemainSeconds 用 FThreadSafeCounter (心跳和倒计时在 GT，
 *     但 GetUserRemainTime 同步读取允许并发安全)。
 */
class FComplianceState
{
public:
	static FComplianceState& Get();

	// 禁拷贝
	FComplianceState(const FComplianceState&) = delete;
	FComplianceState& operator=(const FComplianceState&) = delete;

	// =========================================================
	// 会话 / 用户
	// =========================================================
	FString CurrentUserId;
	FString CurrentSessionId;       // 32 位小写 hex (Guid Digits), 心跳上报用
	FString TrackingSessionId;      // 一次性 Guid, 一次 StartUp 一个, 埋点聚合用
	                                // (对齐 Unity ComplianceNewJob.cs:131 Guid.NewGuid)
	FString ClientId;               // 来自 FTapTapSdkOptions (用于 Persistence 文件夹分组)

	// =========================================================
	// 实名 + 配置
	// =========================================================
	FComplianceVerification Verification;
	FUserAntiConfig         UserAntiConfig;
	FRealNameConfigResult   RealNameConfig;

	// =========================================================
	// 可玩态
	// =========================================================
	FPlayableResult     CurrentPlayableResult;
	FThreadSafeCounter  RemainSeconds;          // 持续递减；< 0 时视为未知 (-1)

	// =========================================================
	// 选项 (对齐 Android TapTapComplianceOptions)
	// =========================================================
	bool bShowSwitchAccount = false;
	bool bUseAgeRange       = true;

	// =========================================================
	// 流程控制
	// =========================================================
	// 当前 StartUp 走过手动实名输入 → 埋点 identity_verify_type=manual_verify, 否则 fast_verify
	// (对齐 Android TapComplianceInternal.kt:341 isManuallyVerify)
	bool bUsedManualVerify = false;

	// =========================================================
	// 回调注册 (持久, 多次触发)
	// =========================================================
	using FComplianceEventCallback = TFunction<void(int32 /*Code*/, const FString& /*ExtraJson*/)>;

	void RegisterCallback(FComplianceEventCallback Callback);
	void UnregisterAllCallbacks();
	void DispatchCallback(int32 Code, const FString& ExtraJson);

#if WITH_DEV_AUTOMATION_TESTS
	/** 测试辅助：替换回调列表并返回旧列表，用于隔离单例副作用。 */
	TArray<FComplianceEventCallback> SwapCallbacksForTest(TArray<FComplianceEventCallback>&& NewCallbacks);
	int64 GetOfflineLocalUnixTimeForTest() const { return OfflineBaseLocalUnixTime; }
	double GetOfflineBaseMonotonicSecondsForTest() const { return OfflineBaseMonotonicSeconds; }
	void SetOfflineLocalTimeForTest(int64 UnixTime, double MonotonicSeconds)
	{
		OfflineBaseLocalUnixTime = UnixTime;
		OfflineBaseMonotonicSeconds = MonotonicSeconds;
	}
#endif

	// =========================================================
	// 工具
	// =========================================================
	/** 生成 32 位小写 hex 随机会话 ID (基于 Guid Digits) */
	static FString GenerateSession32();

	/** GetAgeRange 对外暴露规则 (受 bUseAgeRange + 哨兵映射控制) */
	int32 GetExposedAgeRange() const;

	/** 获取当前用户合规 AccessToken (anti_addiction_token)，未登录时为空 */
	FString GetCurrentAccessToken() const;

	/**
	 * 初始化离线本地时间基准（仅第一次生效）。
	 * 对齐 Unity BaseComplianceWorker.ResetBaseLocalTime：进入缓存兜底/单机模式时，
	 * 捕获一次设备时间，后续用单调时钟推进，避免进游戏后修改系统时间影响判断。
	 */
	void ResetOfflineLocalTimeIfNeeded();

	/** 获取基于离线本地基准 + 单调时钟推进的当前 Unix 时间戳（秒）。 */
	int64 GetOfflineLocalUnixTime();

	/** 重置所有用户态字段 (Logout 用) */
	void ResetUserState();

private:
	FComplianceState();

	TArray<FComplianceEventCallback> Callbacks;
	mutable FCriticalSection         CallbackLock;

	int64  OfflineBaseLocalUnixTime = 0;
	double OfflineBaseMonotonicSeconds = 0.0;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

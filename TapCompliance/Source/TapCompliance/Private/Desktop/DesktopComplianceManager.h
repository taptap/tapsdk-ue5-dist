// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

/**
 * Desktop 合规模块对外门面 (单例)
 *
 * 由 FTapCompliancePlatformDesktop 转发调用：
 *   - 注册 / 注销持久回调
 *   - SetShowSwitchAccount / SetUseAgeRange
 *   - StartUp / Logout
 *   - CheckPaymentLimit / SubmitPayment
 *   - GetAgeRange / GetUserRemainTime
 *
 * 内部协作：
 *   - 持久化与状态写入 FComplianceState / FCompliancePersistence
 *   - 实名编排走 FComplianceAuthFlow
 *   - 心跳轮询走 FCompliancePoll
 *   - 任意位置广播事件统一调 DispatchCallback (内部强切游戏线程)
 */
class FDesktopComplianceManager
{
public:
	static FDesktopComplianceManager& Get();

	// =========================================================
	// 全局回调 / 选项
	// =========================================================
	void RegisterCallback(TFunction<void(int32 /*Code*/, const FString& /*ExtraJson*/)> Callback);
	void UnregisterAllCallbacks();
	void SetShowSwitchAccount(bool bShow);
	void SetUseAgeRange(bool bUse);
	bool UsesAgeRange() const;

	// =========================================================
	// 流程
	// =========================================================
	void StartUp(const FString& UserId);
	void Logout(bool bClearCache);

	// =========================================================
	// 支付
	// =========================================================
	void CheckPaymentLimit(
		int64 AmountFen,
		TFunction<void(int32 /*Status*/, const FString& /*Title*/, const FString& /*Description*/)> OnDone,
		TFunction<void(const FString&)> OnError);

	void SubmitPayment(
		int64 AmountFen,
		TFunction<void(bool /*bSuccess*/, const FString& /*Message*/)> OnDone);

	// =========================================================
	// 同步查询
	// =========================================================
	int32   GetAgeRange();
	int32   GetUserRemainTime();

	// =========================================================
	// 内部分发：被 AuthFlow / Poll / Network 调用
	// =========================================================
	void DispatchCallback(int32 Code, const FString& ExtraJson);

#if WITH_DEV_AUTOMATION_TESTS
	bool IsStartingForTest() const { return bIsStarting; }
	void SetStartingForTest(bool bValue) { bIsStarting = bValue; }
#endif

private:
	FDesktopComplianceManager();
	~FDesktopComplianceManager() = default;

	FDesktopComplianceManager(const FDesktopComplianceManager&) = delete;
	FDesktopComplianceManager& operator=(const FDesktopComplianceManager&) = delete;

	/** 首次使用前从 TapBasic 同步 ClientId 等环境数据到 State */
	void EnsureBootstrap();

	bool bBootstrapped = false;
	bool bIsStarting = false;

	void ClearStartupGuardIfTerminal(int32 Code);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

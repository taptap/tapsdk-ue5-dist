// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Internal/Platform/TapOpenlogTracker.h"

/**
 * 合规模块埋点 (单例, 内部持有 TapCore 的 FTapOpenlogTracker)
 *
 * 对齐 Unity TapComplianceTracker (ComplianceNewJob.cs:131-145) 和
 * Android ComplianceTracker (TapComplianceInternal.kt:161,574,606,639):
 *   - TrackStart    "startup" 入口埋点 (sessionId = 一次性 Guid)
 *   - TrackSuccess  LOGIN_SUCCESS / AGE_LIMIT 成功 (extra: identity_verify_type=fast_verify|manual_verify)
 *   - TrackFailure  INVALID_CLIENT_OR_NETWORK_ERROR / EXITED 失败 (errorCode + errorMsg)
 *   - TrackCancel   REAL_NAME_STOP 用户取消
 *
 * 上报项目名 "TapCompliance" + ComplianceConst::MODULE_VERSION.
 *
 * SessionId 与 FComplianceState.CurrentSessionId (心跳用) 不同源 —
 * 埋点用一次性 Guid, 一次 StartUp 一个, 用于聚合一次启动流程内的所有事件.
 */
class FTapComplianceTracker
{
public:
	static FTapComplianceTracker& Get();

	void TrackInit   () const;
	void TrackStart  (const FString& FuncName, const FString& SessionId) const;
	void TrackSuccess(const FString& FuncName, const FString& SessionId,
	                  const TMap<FString, FString>& Extra = {}) const;
	void TrackFailure(const FString& FuncName, const FString& SessionId,
	                  const FString& ErrorCode, const FString& ErrorMsg,
	                  const TMap<FString, FString>& Extra = {}) const;
	void TrackCancel (const FString& FuncName, const FString& SessionId,
	                  const TMap<FString, FString>& Extra = {}) const;

private:
	FTapComplianceTracker();

	FTapOpenlogTracker Tracker;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#include "TapCompliancePlatformDesktop.h"
#include "TapCompliance.h"
#include "TapComplianceModule.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "DesktopComplianceManager.h"
#include "ComplianceState.h"
#include "Internal/Http/TapHttp.h"
#include "TapBasicTypes.h"

namespace
{
	/** 区域门控：仅大陆可用，否则立即广播 INVALID_CLIENT_OR_NETWORK_ERROR */
	bool EnsureChinaRegion()
	{
		const ETapRegion Region = FTapHttp::GetRegion();
		if (Region == ETapRegion::CN)
		{
			return true;
		}
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Desktop] Compliance is unavailable in non-CN region (current=%d)"),
			static_cast<int32>(Region));
		FDesktopComplianceManager::Get().DispatchCallback(
			TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR,
			TEXT("{\"reason\":\"compliance unavailable in non-CN region\"}"));
		return false;
	}
}

void FTapCompliancePlatformDesktop::ApplyInitOptions(const FTapComplianceOptions& Options)
{
	FDesktopComplianceManager::Get().SetShowSwitchAccount(Options.bShowSwitchAccount);
	FDesktopComplianceManager::Get().SetUseAgeRange(Options.bUseAgeRange);
}

void FTapCompliancePlatformDesktop::RegisterComplianceCallback(
	TFunction<void(int32, const FString&)> Callback)
{
	FDesktopComplianceManager::Get().RegisterCallback(MoveTemp(Callback));
}

void FTapCompliancePlatformDesktop::UnregisterComplianceCallback()
{
	FDesktopComplianceManager::Get().UnregisterAllCallbacks();
}

void FTapCompliancePlatformDesktop::Startup(const FString& UserId)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Desktop] Startup: UserId=%s"), *UserId);
	if (!EnsureChinaRegion())
	{
		return;
	}
	FDesktopComplianceManager::Get().StartUp(UserId);
}

void FTapCompliancePlatformDesktop::Exit()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Desktop] Exit"));
	// 对齐 Unity ComplianceNewJob.cs:160-167 Exit():
	//   1) Logout(true) — 清掉本地 verification + user_anti_config 持久缓存,
	//      下次同账号 Startup 必须重新拉取实名与策略, 避免拿到陈旧数据.
	//   2) DispatchCallback(EXITED) — 主动通知业务方合规会话已结束,
	//      否则游戏侧 UI 没法从"已通过"状态退出, 也拿不到 isCheckingUser 复位时机
	//      (DispatchCallback 负责在游戏线程分发 EXITED 事件).
	FDesktopComplianceManager::Get().Logout(/*bClearCache=*/true);
	FDesktopComplianceManager::Get().DispatchCallback(TapComplianceConst::EXITED, FString());
}

void FTapCompliancePlatformDesktop::CheckPaymentLimit(
	int64 Amount,
	TFunction<void(int32, const FString&, const FString&)> Callback,
	TFunction<void(const FString&)> OnError)
{
	if (!EnsureChinaRegion())
	{
		if (OnError)
		{
			OnError(TEXT("TapCompliance is only supported in CN region"));
		}
		return;
	}
	FDesktopComplianceManager::Get().CheckPaymentLimit(Amount, MoveTemp(Callback), MoveTemp(OnError));
}

void FTapCompliancePlatformDesktop::SubmitPayment(
	int64 Amount,
	TFunction<void(bool, const FString&)> Callback)
{
	if (!EnsureChinaRegion())
	{
		if (Callback)
		{
			Callback(false, TEXT("TapCompliance is only supported in CN region"));
		}
		return;
	}
	FDesktopComplianceManager::Get().SubmitPayment(Amount, MoveTemp(Callback));
}

int32 FTapCompliancePlatformDesktop::GetAgeRange()
{
	return FDesktopComplianceManager::Get().GetAgeRange();
}

int32 FTapCompliancePlatformDesktop::GetUserRemainTime()
{
	return FDesktopComplianceManager::Get().GetUserRemainTime();
}

FString FTapCompliancePlatformDesktop::GetCurrentAccessToken()
{
	return FComplianceState::Get().GetCurrentAccessToken();
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

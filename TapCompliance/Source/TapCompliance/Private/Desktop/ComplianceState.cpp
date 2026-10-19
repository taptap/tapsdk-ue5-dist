// Copyright TapTap. All Rights Reserved.

#include "ComplianceState.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "TapComplianceModule.h"
#include "Async/Async.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"

FComplianceState& FComplianceState::Get()
{
	static FComplianceState Instance;
	return Instance;
}

FComplianceState::FComplianceState()
{
	RemainSeconds.Set(TapComplianceConst::AGE_LIMIT_UNKNOWN);
}

void FComplianceState::RegisterCallback(FComplianceEventCallback Callback)
{
	if (!Callback) return;

	FScopeLock Lock(&CallbackLock);
	Callbacks.Add(MoveTemp(Callback));
	UE_LOG(LogTapCompliance, Log,
		TEXT("[State] Compliance callback registered (total=%d)"), Callbacks.Num());
}

void FComplianceState::UnregisterAllCallbacks()
{
	FScopeLock Lock(&CallbackLock);
	const int32 N = Callbacks.Num();
	Callbacks.Reset();
	UE_LOG(LogTapCompliance, Log,
		TEXT("[State] Unregistered all %d compliance callbacks"), N);
}

void FComplianceState::DispatchCallback(int32 Code, const FString& ExtraJson)
{
	// 拷贝出快照以最小化锁持有时间
	TArray<FComplianceEventCallback> Snapshot;
	{
		FScopeLock Lock(&CallbackLock);
		Snapshot = Callbacks;
	}

	if (Snapshot.Num() == 0)
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[State] DispatchCallback Code=%d but no listener registered"), Code);
		return;
	}

	UE_LOG(LogTapCompliance, Log,
		TEXT("[State] Dispatch compliance event: Code=%d, ExtraJson=%s"),
		Code, ExtraJson.IsEmpty() ? TEXT("(empty)") : *ExtraJson);

	auto Run = [Snapshot = MoveTemp(Snapshot), Code, ExtraJson]()
	{
		for (const FComplianceEventCallback& Cb : Snapshot)
		{
			if (Cb)
			{
				Cb(Code, ExtraJson);
			}
		}
	};

	if (IsInGameThread())
	{
		Run();
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(Run));
	}
}

#if WITH_DEV_AUTOMATION_TESTS
TArray<FComplianceState::FComplianceEventCallback> FComplianceState::SwapCallbacksForTest(
	TArray<FComplianceEventCallback>&& NewCallbacks)
{
	FScopeLock Lock(&CallbackLock);
	TArray<FComplianceEventCallback> OldCallbacks = MoveTemp(Callbacks);
	Callbacks = MoveTemp(NewCallbacks);
	return OldCallbacks;
}
#endif

FString FComplianceState::GenerateSession32()
{
	return FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower();
}

int32 FComplianceState::GetExposedAgeRange() const
{
	if (!bUseAgeRange)
	{
		return TapComplianceConst::AGE_LIMIT_UNKNOWN;
	}
	const int32 Age = Verification.AgeLimit;
	if (Age == TapComplianceConst::AGE_LIMIT_SENTINEL_MINOR
		|| Age == TapComplianceConst::AGE_LIMIT_SENTINEL_ADULT)
	{
		return TapComplianceConst::AGE_LIMIT_UNKNOWN;
	}
	return Age;
}

FString FComplianceState::GetCurrentAccessToken() const
{
	return Verification.ComplianceToken;
}

void FComplianceState::ResetOfflineLocalTimeIfNeeded()
{
	if (OfflineBaseLocalUnixTime > 0)
	{
		return;
	}

	OfflineBaseLocalUnixTime = FDateTime::UtcNow().ToUnixTimestamp();
	OfflineBaseMonotonicSeconds = FPlatformTime::Seconds();
	UE_LOG(LogTapCompliance, Log,
		TEXT("[State] Offline local time baseline set: unix=%lld monotonic=%.3f"),
		OfflineBaseLocalUnixTime, OfflineBaseMonotonicSeconds);
}

int64 FComplianceState::GetOfflineLocalUnixTime()
{
	ResetOfflineLocalTimeIfNeeded();
	const double ElapsedSeconds = FMath::Max(0.0, FPlatformTime::Seconds() - OfflineBaseMonotonicSeconds);
	return OfflineBaseLocalUnixTime + static_cast<int64>(ElapsedSeconds);
}

void FComplianceState::ResetUserState()
{
	CurrentUserId.Empty();
	CurrentSessionId.Empty();
	TrackingSessionId.Empty();
	Verification           = FComplianceVerification();
	UserAntiConfig         = FUserAntiConfig();
	CurrentPlayableResult  = FPlayableResult();
	RemainSeconds.Set(TapComplianceConst::AGE_LIMIT_UNKNOWN);
	bUsedManualVerify      = false;
	OfflineBaseLocalUnixTime = 0;
	OfflineBaseMonotonicSeconds = 0.0;
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

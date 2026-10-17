// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Launcher/TapLauncherTypes.h"
#include "HAL/ThreadSafeBool.h"

#if PLATFORM_WINDOWS

/**
 * Holds launcher verification state and drives the async check flow.
 * Singleton accessed via Get(). Reset() clears state for testing.
 */
class TAPCORE_API FTapLauncherDesktopImpl
{
public:
	static FTapLauncherDesktopImpl& Get();
	static void Reset(); // for tests

	void IsLaunchedFromTapTapPC(
		FOnLauncherCheckSuccess OnSuccess,
		FOnLauncherCheckError   OnError);

	bool IsPassedCheck() const;

	/** Returns the cached OpenID when launcher verification succeeded. */
	bool GetCachedOpenId(FString& OutOpenId) const;

	/** Returns true once launcher verification has started or completed. */
	bool HasStartedCheck() const;

	void Shutdown();

private:
	FTapLauncherDesktopImpl() = default;

	void RunCheckAsync(const FString& ClientId, const FString& PubKey,
	                   FOnLauncherCheckSuccess OnSuccess, FOnLauncherCheckError OnError);
	void FireSuccess(const FString& OpenId, FOnLauncherCheckSuccess OnSuccess);
	void FireError(ETapLauncherInitResult Code, const FString& Msg, FOnLauncherCheckError OnError);

	// State — written from background thread, read from game thread
	FThreadSafeBool         bInProgress{false};
	FThreadSafeBool         bCheckDone{false};
	FThreadSafeBool         bCheckPassed{false};

	mutable FCriticalSection CacheLock;
	FString                  CachedOpenId;
	ETapLauncherInitResult   CachedResult  = ETapLauncherInitResult::Unknown;
	FString                  CachedErrMsg;

	FThreadSafeBool bStateListenerRegistered{false};
};

#endif // PLATFORM_WINDOWS

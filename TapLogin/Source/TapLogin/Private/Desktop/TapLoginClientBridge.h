// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

DECLARE_DELEGATE_TwoParams(FTapLoginClientAuthorizeComplete, bool, const FString&);

/**
 * Thin UE bridge for TapLoginClientBridge's Tap PC authorization API.
 * The bridge owns only copied callback data and permits one request at a time.
 */
class FTapLoginClientBridge
{
public:
	static bool IsAvailable();

	/** Starts one authorization request. Returns false if unavailable or already in flight. */
	static bool Start(
		const FString& ScopeStrings,
		const FString& ResponseType,
		const FString& RedirectUri,
		const FString& CodeChallenge,
		const FString& State,
		const FString& CodeChallengeMethod,
		const FString& VersionCode,
		const FString& SdkUa,
		const FString& Info,
		FTapLoginClientAuthorizeComplete OnComplete);

	/** Invalidates the current request and unregisters the native callback. */
	static void Cancel();

	/** Cancels any request before the launcher DLL is unloaded. */
	static void Shutdown();
};

#endif // PLATFORM_WINDOWS

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internal/Platform/TapOpenlogTracker.h"

/**
 * Tracker for the TapLogin module.
 *
 * Singleton; reports login-related business events via FTapOpenlog.
 * Mirrors the Unity TapLoginTracker (TapSDK.Login.Standalone.Internal).
 */
class TAPLOGIN_API FTapLoginTracker
{
public:
	static FTapLoginTracker& Get();

	/** Report that TapLogin was successfully initialised. */
	void TrackInit();

	/**
	 * Report a "start" event for a login operation.
	 * @param FuncName   Name of the function (e.g. "loginWithScopes").
	 * @param SessionId  Unique session identifier.
	 * @param LoginType  Optional login type (pc_code / pc_browser).
	 */
	void TrackStart(const FString& FuncName, const FString& SessionId, const FString& LoginType = TEXT(""));

	/**
	 * Report a "success" event.
	 * @param FuncName   Name of the function.
	 * @param SessionId  Session identifier.
	 * @param LoginType  Login type (pc_code / pc_browser).
	 */
	void TrackSuccess(const FString& FuncName, const FString& SessionId, const FString& LoginType);

	/**
	 * Report a "fail" event.
	 * @param FuncName   Name of the function.
	 * @param SessionId  Session identifier.
	 * @param LoginType  Login type.
	 * @param ErrorCode  Numeric error code.
	 * @param ErrorMsg   Human-readable error message.
	 */
	void TrackFailure(const FString& FuncName, const FString& SessionId,
	                  const FString& LoginType, int32 ErrorCode, const FString& ErrorMsg);

	/**
	 * Report a "cancel" event.
	 * @param FuncName   Name of the function.
	 * @param SessionId  Session identifier.
	 * @param LoginType  Optional login type.
	 */
	void TrackCancel(const FString& FuncName, const FString& SessionId, const FString& LoginType = TEXT(""));

	// Login type constants (matching Unity TapLoginTracker)
	static const FString LOGIN_TYPE_CODE;     // "pc_code"
	static const FString LOGIN_TYPE_BROWSER;  // "pc_browser"

private:
	FTapLoginTracker();

	FTapOpenlogTracker Tracker;
};

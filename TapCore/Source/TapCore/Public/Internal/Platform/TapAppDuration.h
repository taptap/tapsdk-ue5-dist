// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * AppDuration module helper for Desktop (Win64/Mac/Linux).
 *
 * Thin wrapper around the tapsdkcore "app_duration" module that
 * tracks how long the user plays the game and sends duration events
 * to the OpenLog backend.
 *
 * All calls are no-ops if the native library has not been loaded.
 */
class TAPCORE_API FTapAppDuration
{
public:
	/** Enable the app_duration module (call after FTapOpenlog::Init). */
	static void Enable();

	/** Disable the app_duration module. */
	static void Disable();

	/**
	 * Notify the SDK that a user has logged in.
	 * @param OpenId  TapTap open_id of the logged-in user.
	 */
	static void OnLogin(const FString& OpenId);

	/** Notify the SDK that the current user has logged out. */
	static void OnLogout();

	/**
	 * Set extra key/value parameters to be included in duration events.
	 * The new params replace any previously set params.
	 * @param Params  JSON object of extra parameters.
	 */
	static void SetExtraParams(const TMap<FString, FString>& Params);
};

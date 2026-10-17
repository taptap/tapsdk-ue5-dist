// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

class FTapLoginDesktopWebAuth
{
public:
	static FString BuildAuthorizationURL(
		const FString& BaseURL,
		const FString& ClientID,
		const FString& RedirectURI,
		const FString& State,
		const FString& CodeChallenge,
		const FString& Scope);

	static bool TryParseCallbackPath(
		const FString& Path,
		const FString& ExpectedState,
		FString& OutCode);

	/**
	 * Parses a browser OAuth callback, including an authorization error.
	 * OutError is populated when the provider returns an error such as access_denied.
	 */
	static bool TryParseCallbackPath(
		const FString& Path,
		const FString& ExpectedState,
		FString& OutCode,
		FString& OutError);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

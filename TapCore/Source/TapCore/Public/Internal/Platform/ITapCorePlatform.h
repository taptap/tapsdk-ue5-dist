// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"

/**
 * Platform Interface for TapSDK Core
 * All platform-specific implementations must inherit from this interface
 */
class ITapCorePlatform
{
public:
	virtual ~ITapCorePlatform() = default;

	/**
	 * Initialize platform-specific TapSDK implementation
	 * @param Config TapSDK configuration
	 */
	virtual void Init(const FTapTapSdkOptions& Config) = 0;

	/**
	 * Get platform-specific SDK version
	 * @return Version string
	 */
	virtual FString GetPlatformVersion() const = 0;

	/**
	 * Check if platform SDK is initialized
	 * @return true if initialized
	 */
	virtual bool IsInitialized() const = 0;

	/** Apply the final result; accepting the native request is not initialization success. */
	virtual void OnInitResult(bool bSuccess) {}

	/**
	 * Get platform name
	 * @return Platform name (Android/iOS/Desktop)
	 */
	virtual FString GetPlatformName() const = 0;

	/**
	 * Update language setting
	 * @param Language Target language to switch to
	 */
	virtual void UpdateLanguage(ETapLanguage Language) = 0;

	/**
	 * Request Gatekeeper validation (platform-specific HTTP implementation)
	 * @param Config TapSDK configuration for request parameters
	 */
	virtual void RequestGatekeeperValidation(const FTapTapSdkOptions& Config) = 0;

	/**
	 * Get config URL from gatekeeper response by key.
	 * Returns empty string if key not found or gatekeeper hasn't completed.
	 * Desktop overrides this; mobile platforms return empty string (URL unused on mobile).
	 */
	virtual FString GetGatekeeperConfigUrl(const FString& Key) const { return FString(); }
};

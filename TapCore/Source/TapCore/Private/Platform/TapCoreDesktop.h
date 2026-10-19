// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" // Must precede platform checks; PLATFORM_* macros come from headers, not compiler flags

// Only compile on Desktop platforms (Windows/Mac/Linux)
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Internal/Platform/ITapCorePlatform.h"

class FTapHttp;

/**
 * Desktop Platform Implementation for TapSDK Core
 * May use HTTP API or desktop-specific SDK
 */
class TAPCORE_API FTapCoreDesktop : public ITapCorePlatform
{
public:
	FTapCoreDesktop();
	virtual ~FTapCoreDesktop();

	// ITapCorePlatform interface
	virtual void Init(const FTapTapSdkOptions& Config) override;
	virtual FString GetPlatformVersion() const override;
	virtual bool IsInitialized() const override;
	virtual FString GetPlatformName() const override;
	virtual void UpdateLanguage(ETapLanguage Language) override;
	virtual void RequestGatekeeperValidation(const FTapTapSdkOptions& Config) override;
	virtual FString GetGatekeeperConfigUrl(const FString& Key) const override;

	/** Apply the cached Gatekeeper heartbeat setting before the network response arrives. */
	static void ApplyCachedHeartbeat(const FTapTapSdkOptions& Config);

private:
	/** Initialize HTTP-based communication */
	void InitializeHTTP(const FTapTapSdkOptions& Config);

	/** Send initialization request to server */
	void SendInitRequest(const FTapTapSdkOptions& Config);

	/** Initialized flag */
	bool bIsInitialized;

	/** Stored config */
	FTapTapSdkOptions CurrentConfig;

	/** Gatekeeper HTTP client — must outlive async request */
	TSharedPtr<FTapHttp> GatekeeperHttpClient;

	/** Config URLs received from gatekeeper response (e.g. "achievement_my_list_url") */
	TMap<FString, FString> GatekeeperConfig;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

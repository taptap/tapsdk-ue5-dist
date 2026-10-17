// Copyright TapTap. All Rights Reserved.

#pragma once

// Only compile on iOS platform
#if PLATFORM_IOS

#include "CoreMinimal.h"
#include "Internal/Platform/ITapCorePlatform.h"

/**
 * iOS Platform Implementation for TapSDK Core
 * Uses Objective-C++ to bridge to iOS Native Framework
 */
class TAPCORE_API FTapCoreiOS : public ITapCorePlatform
{
public:
	FTapCoreiOS();
	virtual ~FTapCoreiOS();

	// ITapCorePlatform interface
	virtual void Init(const FTapTapSdkOptions& Config) override;
	virtual FString GetPlatformVersion() const override;
	virtual bool IsInitialized() const override;
	virtual void OnInitResult(bool bSuccess) override { bIsInitialized = bSuccess; }
	virtual FString GetPlatformName() const override;
	virtual void UpdateLanguage(ETapLanguage Language) override;
	virtual void RequestGatekeeperValidation(const FTapTapSdkOptions& Config) override;

private:
	/** Call iOS Native Framework init method */
	void CalliOSInit(const FTapTapSdkOptions& Config);

	/** Convert FTapTapSdkOptions to JSON string for iOS bridge */
	FString ConvertConfigToJson(const FTapTapSdkOptions& Config) const;

	/** Convert FTapTapSdkOptions to iOS SDK config object (deprecated, use JSON instead) */
	void* CreateiOSConfig(const FTapTapSdkOptions& Config) const;

	/** Initialized flag */
	bool bIsInitialized;

	/** Stored configuration */
	FTapTapSdkOptions CurrentConfig;
};

#endif // PLATFORM_IOS

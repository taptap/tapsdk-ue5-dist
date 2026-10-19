// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h" // Must precede platform checks; PLATFORM_* macros come from headers, not compiler flags

// Only compile on Desktop platforms
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Platform/ITapEventPlatform.h"

/**
 * Desktop Platform Implementation for TapEvent
 * Provides mock implementation for development and testing
 */
class TAPCORE_API FTapEventDesktop : public ITapEventPlatform
{
public:
	FTapEventDesktop();
	virtual ~FTapEventDesktop();

	// ITapEventPlatform interface
	virtual void Init(bool bEnableLog) override;
	virtual void SetUserID(const FString& UserID) override;
	virtual void SetUserID(const FString& UserID, const FString& Properties) override;
	virtual void ClearUser() override;
	virtual FString GetDeviceId() override;
	virtual void LogEvent(const FString& Name, const FString& Properties) override;
	virtual void DeviceInitialize(const FString& Properties) override;
	virtual void DeviceUpdate(const FString& Properties) override;
	virtual void DeviceAdd(const FString& Properties) override;
	virtual void UserInitialize(const FString& Properties) override;
	virtual void UserUpdate(const FString& Properties) override;
	virtual void UserAdd(const FString& Properties) override;
	virtual void AddCommonProperty(const FString& Key, const FString& Value) override;
	virtual void AddCommon(const FString& Properties) override;
	virtual void ClearCommonProperty(const FString& Key) override;
	virtual void ClearCommonProperties(const TArray<FString>& Keys) override;
	virtual void ClearAllCommonProperties() override;
	virtual void LogPurchasedEvent(const FString& OrderJson) override;
	virtual void RegisterDynamicProperties(TFunction<FString()> Callback) override;
	virtual void SetOAID(const FString& Value) override;
	virtual void LogDeviceLoginEvent() override;

private:
	bool bIsInitialized;
	bool bEnableLog;
	TFunction<FString()> DynamicPropertiesCallback;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

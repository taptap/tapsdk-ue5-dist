// Copyright TapTap. All Rights Reserved.

#pragma once

// Only compile on Android platform
#if PLATFORM_ANDROID

#include "CoreMinimal.h"
#include "Platform/ITapEventPlatform.h"

/**
 * Android Platform Implementation for TapEvent
 * Uses JNI to bridge to Android Native SDK via Router pattern
 */
class TAPCORE_API FTapEventAndroid : public ITapEventPlatform
{
public:
	FTapEventAndroid();
	virtual ~FTapEventAndroid();

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
	TFunction<FString()> DynamicPropertiesCallback;
};

#endif // PLATFORM_ANDROID

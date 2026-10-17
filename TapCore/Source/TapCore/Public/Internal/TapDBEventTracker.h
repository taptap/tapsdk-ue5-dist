// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "TapBasicTypes.h"
#include "Internal/TapDBIdentity.h"
#include "Internal/TapDBUserManager.h"
#include "Internal/TapDBEventSender.h"

class FTapMachineBoundPrefs;

/**
 * Desktop-only event engine used by FTapEventDesktop.
 * Game code should call FTapTapEvent, matching Unity TapTapEvent.
 */
class TAPCORE_API FTapDBEventTracker
{
public:
	static FTapDBEventTracker& Get();

	void Init(const FString& ClientID, const FString& ClientToken,
		ETapRegion Region, bool bEnableLog,
		const FString& GameVersion, const FString& Channel,
		bool bDisableAutoLogDeviceLogin = false,
		bool bOverrideBuiltInParameters = false,
		const FString& InitProperties = TEXT(""));
	void Shutdown();
	bool IsInitialized() const { return bIsInitialized; }

	// === User ===
	void SetUserID(const FString& UserID);
	void SetUserID(const FString& UserID, const FString& PropertiesJson);
	void ClearUser();
	FString GetDeviceId();

	// === Events ===
	void LogEvent(const FString& Name, const FString& PropertiesJson);
	void LogPurchasedEvent(const FString& OrderJson);
	void LogDeviceLoginEvent();

	// === Device Properties ===
	void DeviceInitialize(const FString& PropertiesJson);
	void DeviceUpdate(const FString& PropertiesJson);
	void DeviceAdd(const FString& PropertiesJson);

	// === User Properties ===
	void UserInitialize(const FString& PropertiesJson);
	void UserUpdate(const FString& PropertiesJson);
	void UserAdd(const FString& PropertiesJson);

	// === Common Properties ===
	void AddCommonProperty(const FString& Key, const FString& Value);
	void AddCommon(const FString& PropertiesJson);
	void ClearCommonProperty(const FString& Key);
	void ClearCommonProperties(const TArray<FString>& Keys);
	void ClearAllCommonProperties();

	// === Dynamic Properties ===
	void RegisterDynamicProperties(TFunction<FString()> Callback);

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTapDBCommonPropertiesTypesTest;
#endif

	FTapDBEventTracker();
	~FTapDBEventTracker();

	// Non-copyable
	FTapDBEventTracker(const FTapDBEventTracker&) = delete;
	FTapDBEventTracker& operator=(const FTapDBEventTracker&) = delete;

	// Validate user ID format
	bool IsValidUserID(const FString& UserID) const;
	// Validate event name
	bool IsValidEventName(const FString& Name) const;

	// Parse JSON string to object
	TSharedPtr<FJsonObject> ParseJsonString(const FString& JsonString) const;

	// Build a track event with property merging
	TSharedPtr<FJsonObject> BuildTrackEvent(const FString& Name,
		TSharedPtr<FJsonObject> UserProps, bool bIsAutomatic);

	// Build a property change event (device/user initialise/update/add)
	TSharedPtr<FJsonObject> BuildPropertyEvent(const FString& Type,
		TSharedPtr<FJsonObject> Props, bool bIsDevice);

	// Callback from UserManager when play_game event should fire
	void OnPlayGameEvent(const FString& UserID, int64 Duration);

	// Get current language string
	FString GetSDKLocale() const;
	FString GetSystemLanguage() const;

	// Components
	TUniquePtr<FTapMachineBoundPrefs> Prefs;
	FTapDBIdentity Identity;
	FTapDBUserManager UserManager;
	FTapDBEventSender EventSender;

	// State
	FString ClientID;
	FString ClientToken;
	bool bIsInitialized;
	bool bEnableLog;
	bool bOverrideBuiltInParameters = false;
	int64 EventIndex = 0;

	// Properties
	TSharedPtr<FJsonObject> BasicProperties;
	// Keep common properties as JSON values so numbers, booleans, arrays and objects
	// are not coerced to strings or dropped before event serialization.
	TMap<FString, TSharedPtr<FJsonValue>> CommonProperties;
	TFunction<FString()> DynamicPropsCallback;
};

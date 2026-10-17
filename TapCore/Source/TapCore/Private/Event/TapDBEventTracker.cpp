// Copyright TapTap. All Rights Reserved.

#include "Internal/TapDBEventTracker.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/TapDBConstants.h"
#include "TapDBDeviceInfo.h"
#include "TapCoreModule.h"
#include "Misc/Guid.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TapJsonCompat.h"

FTapDBEventTracker::FTapDBEventTracker()
	: Prefs(nullptr)
	, bIsInitialized(false)
	, bEnableLog(false)
{
}

FTapDBEventTracker::~FTapDBEventTracker()
{
	Shutdown();
}

FTapDBEventTracker& FTapDBEventTracker::Get()
{
	static FTapDBEventTracker Instance;
	return Instance;
}

void FTapDBEventTracker::Init(const FString& InClientID, const FString& InClientToken,
	ETapRegion Region, bool bInEnableLog,
	const FString& GameVersion, const FString& Channel,
	bool bDisableAutoLogDeviceLogin,
	bool bInOverrideBuiltInParameters,
	const FString& InitProperties)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventTracker] Already initialized"));
		return;
	}

	ClientID = InClientID;
	ClientToken = InClientToken;
	bEnableLog = bInEnableLog;
	bOverrideBuiltInParameters = bInOverrideBuiltInParameters;

	// Init persistence (path relative to ProjectSavedDir)
	Prefs = MakeUnique<FTapMachineBoundPrefs>();
	Prefs->Init(FString::Printf(TEXT("TapDB/tapdb_storage_%s.json"), *ClientID));

	// Init identity
	Identity.Init(*Prefs);

	// Init event sender
	EventSender.Init(ClientID, Region);

	// Machine changed — discard old machine's unsent events
	if (Prefs->WasInvalidated())
	{
		EventSender.ClearCache();
	}

	// Init user manager
	UserManager.Init(*Prefs);
	UserManager.OnPlayGameEvent.BindRaw(this, &FTapDBEventTracker::OnPlayGameEvent);

	// Collect basic device properties
	FString SDKVersion = TEXT("1.0.0");
#ifdef TapCore_VERSION
	SDKVersion = TapCore_VERSION;
#endif
	BasicProperties = FTapDBDeviceInfo::CollectBasicProperties(
		Identity, GameVersion, Channel, SDKVersion);

	// Merge init properties into basic properties (for device_login event)
	if (!InitProperties.IsEmpty())
	{
		TSharedPtr<FJsonObject> InitProps = ParseJsonString(InitProperties);
		if (InitProps.IsValid() && BasicProperties.IsValid())
		{
			for (const auto& Pair : InitProps->Values)
			{
				const FString Key = TapJson::KeyString(Pair.Key);
				if (bOverrideBuiltInParameters || !BasicProperties->HasField(Key))
				{
					BasicProperties->SetField(Key, Pair.Value);
				}
			}
		}
	}

	bIsInitialized = true;

	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] Initialized: ClientID=%s, Region=%d, EnableLog=%s, OverrideBuiltIn=%s, DisableAutoDeviceLogin=%s"),
		*ClientID, static_cast<int32>(Region),
		bEnableLog ? TEXT("true") : TEXT("false"),
		bOverrideBuiltInParameters ? TEXT("true") : TEXT("false"),
		bDisableAutoLogDeviceLogin ? TEXT("true") : TEXT("false"));

	// Auto-send device_login event (can be disabled via config)
	if (!bDisableAutoLogDeviceLogin)
	{
		LogDeviceLoginEvent();
	}
	else
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] Auto device_login disabled by config"));
	}
}

void FTapDBEventTracker::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	bIsInitialized = false;

	UserManager.Shutdown();
	EventSender.Shutdown();

	Prefs.Reset();

	BasicProperties.Reset();
	CommonProperties.Empty();
	DynamicPropsCallback = nullptr;
	EventIndex = 0;

	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] Shutdown"));
}

// === User ===

void FTapDBEventTracker::SetUserID(const FString& UserID)
{
	SetUserID(UserID, TEXT(""));
}

void FTapDBEventTracker::SetUserID(const FString& UserID, const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	if (!IsValidUserID(UserID))
	{
		UE_LOG(LogTapDB, Error, TEXT("[TapDBEventTracker] Invalid UserID: %s"), *UserID);
		return;
	}

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	UserManager.Login(UserID, Props);

	// Send user_login event
	TSharedPtr<FJsonObject> Event = BuildTrackEvent(
		TapDBConstants::EVENT_USER_LOGIN, Props, true);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}

	// Update device property: has_user = true
	TSharedPtr<FJsonObject> DeviceProps = MakeShared<FJsonObject>();
	DeviceProps->SetBoolField(TapDBConstants::PROP_HAS_USER, true);
	TSharedPtr<FJsonObject> DeviceEvent = BuildPropertyEvent(
		TapDBConstants::TYPE_UPDATE, DeviceProps, true);
	if (DeviceEvent.IsValid())
	{
		EventSender.Send(DeviceEvent);
	}

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] SetUserID: %s"), *UserID);
	}
}

void FTapDBEventTracker::ClearUser()
{
	if (!bIsInitialized) return;

	UserManager.Logout();

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] ClearUser"));
	}
}

FString FTapDBEventTracker::GetDeviceId()
{
	return Identity.GetDeviceId();
}

// === Events ===

void FTapDBEventTracker::LogEvent(const FString& Name, const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	if (!IsValidEventName(Name))
	{
		UE_LOG(LogTapDB, Error, TEXT("[TapDBEventTracker] Invalid event name: %s"), *Name);
		return;
	}

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	TSharedPtr<FJsonObject> Event = BuildTrackEvent(Name, Props, false);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] LogEvent: %s"), *Name);
	}
}

void FTapDBEventTracker::LogPurchasedEvent(const FString& OrderJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> OrderObj = ParseJsonString(OrderJson);
	if (!OrderObj.IsValid())
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventTracker] LogPurchasedEvent: invalid JSON"));
		return;
	}

	// Extract purchase fields and build properties
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

	FString TempStr;
	if (OrderObj->TryGetStringField(TEXT("orderId"), TempStr))
	{
		Props->SetStringField(TEXT("order_id"), TempStr);
	}
	if (OrderObj->TryGetStringField(TEXT("productName"), TempStr))
	{
		Props->SetStringField(TEXT("product"), TempStr);
	}
	double TempNum = 0;
	if (OrderObj->TryGetNumberField(TEXT("amount"), TempNum))
	{
		Props->SetNumberField(TEXT("amount"), TempNum);
	}
	if (OrderObj->TryGetStringField(TEXT("currencyType"), TempStr))
	{
		Props->SetStringField(TEXT("currency_type"), TempStr);
	}
	if (OrderObj->TryGetStringField(TEXT("paymentMethod"), TempStr))
	{
		Props->SetStringField(TEXT("payment"), TempStr);
	}

	// Merge extra properties if present
	const TSharedPtr<FJsonObject>* ExtraProps;
	if (OrderObj->TryGetObjectField(TEXT("properties"), ExtraProps) && ExtraProps->IsValid())
	{
		for (const auto& Pair : (*ExtraProps)->Values)
		{
			Props->SetField(TapJson::KeyString(Pair.Key), Pair.Value);
		}
	}

	TSharedPtr<FJsonObject> Event = BuildTrackEvent(TapDBConstants::EVENT_CHARGE, Props, false);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] LogPurchasedEvent"));
	}
}

void FTapDBEventTracker::LogDeviceLoginEvent()
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Event = BuildTrackEvent(
		TapDBConstants::EVENT_DEVICE_LOGIN, nullptr, true);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] LogDeviceLoginEvent"));
	}
}

// === Device Properties ===

void FTapDBEventTracker::DeviceInitialize(const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	TSharedPtr<FJsonObject> Event = BuildPropertyEvent(TapDBConstants::TYPE_INITIALISE, Props, true);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}
}

void FTapDBEventTracker::DeviceUpdate(const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	TSharedPtr<FJsonObject> Event = BuildPropertyEvent(TapDBConstants::TYPE_UPDATE, Props, true);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}
}

void FTapDBEventTracker::DeviceAdd(const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	TSharedPtr<FJsonObject> Event = BuildPropertyEvent(TapDBConstants::TYPE_ADD, Props, true);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}
}

// === User Properties ===

void FTapDBEventTracker::UserInitialize(const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	TSharedPtr<FJsonObject> Event = BuildPropertyEvent(TapDBConstants::TYPE_INITIALISE, Props, false);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}
}

void FTapDBEventTracker::UserUpdate(const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	TSharedPtr<FJsonObject> Event = BuildPropertyEvent(TapDBConstants::TYPE_UPDATE, Props, false);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}
}

void FTapDBEventTracker::UserAdd(const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	TSharedPtr<FJsonObject> Event = BuildPropertyEvent(TapDBConstants::TYPE_ADD, Props, false);
	if (Event.IsValid())
	{
		EventSender.Send(Event);
	}
}

// === Common Properties ===

void FTapDBEventTracker::AddCommonProperty(const FString& Key, const FString& Value)
{
	if (!bIsInitialized) return;
	if (Key.IsEmpty()) return;
	CommonProperties.Add(Key, MakeShared<FJsonValueString>(Value));

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] AddCommonProperty: key=%s, total=%d"), *Key, CommonProperties.Num());
	}
}

void FTapDBEventTracker::AddCommon(const FString& PropertiesJson)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = ParseJsonString(PropertiesJson);
	if (Props.IsValid())
	{
		for (const auto& Pair : Props->Values)
		{
			const FString Key = TapJson::KeyString(Pair.Key);
			if (Key.IsEmpty()) continue;
			CommonProperties.Add(Key, Pair.Value);
		}
	}

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] AddCommon: total=%d"), CommonProperties.Num());
		for (const auto& Pair : CommonProperties)
		{
			UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker]   key=%s"), *Pair.Key);
		}
	}
}

void FTapDBEventTracker::ClearCommonProperty(const FString& Key)
{
	if (!bIsInitialized) return;
	int32 Removed = CommonProperties.Remove(Key);

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] ClearCommonProperty: key=%s, removed=%d, remaining=%d"),
			*Key, Removed, CommonProperties.Num());
	}
}

void FTapDBEventTracker::ClearCommonProperties(const TArray<FString>& Keys)
{
	if (!bIsInitialized) return;

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] ClearCommonProperties: %d keys to remove, current total=%d"),
			Keys.Num(), CommonProperties.Num());
	}

	for (const FString& Key : Keys)
	{
		int32 Removed = CommonProperties.Remove(Key);
		if (bEnableLog)
		{
			UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker]   remove key=[%s] (len=%d), result=%d"),
				*Key, Key.Len(), Removed);
		}
	}

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] ClearCommonProperties done, remaining=%d"), CommonProperties.Num());
		for (const auto& Pair : CommonProperties)
		{
			UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker]   remaining key=%s"), *Pair.Key);
		}
	}
}

void FTapDBEventTracker::ClearAllCommonProperties()
{
	if (!bIsInitialized) return;
	CommonProperties.Empty();

	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] ClearAllCommonProperties"));
	}
}

// === Dynamic Properties ===

void FTapDBEventTracker::RegisterDynamicProperties(TFunction<FString()> Callback)
{
	DynamicPropsCallback = Callback;
}

// === Private Methods ===

bool FTapDBEventTracker::IsValidUserID(const FString& UserID) const
{
	if (UserID.IsEmpty() || UserID.Len() > 160)
	{
		return false;
	}

	// Validate: ^[a-zA-Z0-9_\-+/=.,:]{1,160}$ (aligned with Android UserIdVerifyTools)
	// Use ASCII-only checks to match Android's regex behavior
	for (TCHAR Ch : UserID)
	{
		const bool bIsAsciiAlnum = (Ch >= TEXT('a') && Ch <= TEXT('z')) ||
			(Ch >= TEXT('A') && Ch <= TEXT('Z')) ||
			(Ch >= TEXT('0') && Ch <= TEXT('9'));
		if (!bIsAsciiAlnum &&
			Ch != TEXT('_') && Ch != TEXT('-') && Ch != TEXT('+') && Ch != TEXT('/') &&
			Ch != TEXT('=') && Ch != TEXT('.') && Ch != TEXT(',') &&
			Ch != TEXT(':'))
		{
			return false;
		}
	}
	return true;
}

bool FTapDBEventTracker::IsValidEventName(const FString& Name) const
{
	return !Name.IsEmpty() && Name.Len() <= TapDBConstants::MAX_EVENT_NAME_LENGTH;
}

TSharedPtr<FJsonObject> FTapDBEventTracker::ParseJsonString(const FString& JsonString) const
{
	if (JsonString.IsEmpty())
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Result;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (FJsonSerializer::Deserialize(Reader, Result))
	{
		return Result;
	}

	UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventTracker] Failed to parse JSON: %s"), *JsonString);
	return nullptr;
}

TSharedPtr<FJsonObject> FTapDBEventTracker::BuildTrackEvent(const FString& Name,
	TSharedPtr<FJsonObject> UserProps, bool bIsAutomatic)
{
	TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();

	// Top-level fields
	Event->SetStringField(TEXT("client_id"), ClientID);
	Event->SetStringField(TEXT("type"), TapDBConstants::TYPE_TRACK);
	Event->SetStringField(TEXT("name"), Name);
	Event->SetStringField(TEXT("device_id"), Identity.GetDeviceId());

	// User ID if logged in
	FString CurrentUserID = UserManager.GetCurrentUserID();
	if (!CurrentUserID.IsEmpty())
	{
		Event->SetStringField(TEXT("user_id"), CurrentUserID);
	}

	// Build properties with merging priority:
	// BasicProperties < CommonProperties < DynamicProperties < UserProps
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

	// 1. Basic properties (lowest priority)
	if (BasicProperties.IsValid())
	{
		for (const auto& Pair : BasicProperties->Values)
		{
			Props->SetField(TapJson::KeyString(Pair.Key), Pair.Value);
		}
	}

	// 2. Common properties
	if (bEnableLog)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] BuildTrackEvent(%s): merging %d common properties"), *Name, CommonProperties.Num());
		for (const auto& Pair : CommonProperties)
		{
			UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker]   common: %s"), *Pair.Key);
		}
	}
	for (const auto& Pair : CommonProperties)
	{
		Props->SetField(Pair.Key, Pair.Value);
	}

	// 3. Dynamic properties
	if (DynamicPropsCallback)
	{
		FString DynamicJson = DynamicPropsCallback();
		TSharedPtr<FJsonObject> DynamicProps = ParseJsonString(DynamicJson);
		if (DynamicProps.IsValid())
		{
			for (const auto& Pair : DynamicProps->Values)
			{
				Props->SetField(TapJson::KeyString(Pair.Key), Pair.Value);
			}
		}
	}

	// 4. System properties (always set)
	const FDateTime Now = FDateTime::UtcNow();
	Props->SetStringField(TapDBConstants::PROP_T_LOG_ID,
		FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	Props->SetNumberField(TapDBConstants::PROP_TIMESTAMP,
		static_cast<double>(Now.ToUnixTimestamp() * 1000LL + Now.GetMillisecond()));
	Props->SetStringField(TapDBConstants::PROP_IS_AUTO_LOG,
		bIsAutomatic ? TEXT("true") : TEXT("false"));
	Props->SetNumberField(TapDBConstants::PROP_EVENT_INDEX, static_cast<double>(EventIndex++));
	Props->SetStringField(TapDBConstants::PROP_SDK_LOCALE, GetSDKLocale());
	Props->SetStringField(TapDBConstants::PROP_LANG_SYSTEM, GetSystemLanguage());

	// 5. User-provided properties (highest priority)
	if (UserProps.IsValid())
	{
		for (const auto& Pair : UserProps->Values)
		{
			const FString Key = TapJson::KeyString(Pair.Key);
			if (!Key.IsEmpty())
			{
				Props->SetField(Key, Pair.Value);
			}
		}
	}

	Event->SetObjectField(TEXT("properties"), Props);

	return Event;
}

TSharedPtr<FJsonObject> FTapDBEventTracker::BuildPropertyEvent(const FString& Type,
	TSharedPtr<FJsonObject> Props, bool bIsDevice)
{
	TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();

	Event->SetStringField(TEXT("client_id"), ClientID);
	Event->SetStringField(TEXT("type"), Type);

	if (bIsDevice)
	{
		Event->SetStringField(TEXT("device_id"), Identity.GetDeviceId());
	}
	else
	{
		FString CurrentUserID = UserManager.GetCurrentUserID();
		if (CurrentUserID.IsEmpty())
		{
			UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventTracker] User property event requires logged-in user"));
			return nullptr;
		}
		Event->SetStringField(TEXT("user_id"), CurrentUserID);
	}

	// Properties
	TSharedPtr<FJsonObject> FinalProps = MakeShared<FJsonObject>();

	// Always include sdk_version
	FString SDKVersion = TEXT("1.0.0");
#ifdef TapCore_VERSION
	SDKVersion = TapCore_VERSION;
#endif
	FinalProps->SetStringField(TapDBConstants::PROP_SDK_VERSION, SDKVersion);

	// Merge user-provided properties
	if (Props.IsValid())
	{
		for (const auto& Pair : Props->Values)
		{
			const FString Key = TapJson::KeyString(Pair.Key);
			if (!Key.IsEmpty())
			{
				FinalProps->SetField(Key, Pair.Value);
			}
		}
	}

	Event->SetObjectField(TEXT("properties"), FinalProps);

	return Event;
}

void FTapDBEventTracker::OnPlayGameEvent(const FString& UserID, int64 Duration)
{
	if (!bIsInitialized) return;

	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetNumberField(TapDBConstants::PROP_DURATION, static_cast<double>(Duration));

	// Build event with the user ID that was logged out
	TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("client_id"), ClientID);
	Event->SetStringField(TEXT("type"), TapDBConstants::TYPE_TRACK);
	Event->SetStringField(TEXT("name"), TapDBConstants::EVENT_PLAY_GAME);
	Event->SetStringField(TEXT("device_id"), Identity.GetDeviceId());
	Event->SetStringField(TEXT("user_id"), UserID);

	// Build properties
	TSharedPtr<FJsonObject> EventProps = MakeShared<FJsonObject>();

	// Basic properties
	if (BasicProperties.IsValid())
	{
		for (const auto& Pair : BasicProperties->Values)
		{
			EventProps->SetField(TapJson::KeyString(Pair.Key), Pair.Value);
		}
	}

	// System fields
	const FDateTime Now = FDateTime::UtcNow();
	EventProps->SetStringField(TapDBConstants::PROP_T_LOG_ID,
		FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));
	EventProps->SetNumberField(TapDBConstants::PROP_TIMESTAMP,
		static_cast<double>(Now.ToUnixTimestamp() * 1000LL + Now.GetMillisecond()));
	EventProps->SetStringField(TapDBConstants::PROP_IS_AUTO_LOG, TEXT("true"));
	EventProps->SetNumberField(TapDBConstants::PROP_EVENT_INDEX, static_cast<double>(EventIndex++));
	EventProps->SetStringField(TapDBConstants::PROP_SDK_LOCALE, GetSDKLocale());
	EventProps->SetStringField(TapDBConstants::PROP_LANG_SYSTEM, GetSystemLanguage());

	// Duration
	EventProps->SetNumberField(TapDBConstants::PROP_DURATION, static_cast<double>(Duration));

	Event->SetObjectField(TEXT("properties"), EventProps);
	EventSender.Send(Event);

	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventTracker] play_game event: user=%s, duration=%lld s"),
		*UserID, Duration);
}

FString FTapDBEventTracker::GetSDKLocale() const
{
	FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	// Convert to simple language code
	if (Culture.StartsWith(TEXT("zh")))
	{
		if (Culture.Contains(TEXT("Hant")) || Culture.Contains(TEXT("TW")) || Culture.Contains(TEXT("HK")))
		{
			return TEXT("zh_Hant");
		}
		return TEXT("zh_Hans");
	}
	if (Culture.StartsWith(TEXT("ja"))) return TEXT("ja");
	if (Culture.StartsWith(TEXT("ko"))) return TEXT("ko");
	if (Culture.StartsWith(TEXT("en"))) return TEXT("en");
	if (Culture.StartsWith(TEXT("de"))) return TEXT("de");
	if (Culture.StartsWith(TEXT("es"))) return TEXT("es");
	if (Culture.StartsWith(TEXT("fr"))) return TEXT("fr");
	if (Culture.StartsWith(TEXT("pt"))) return TEXT("pt");
	if (Culture.StartsWith(TEXT("ru"))) return TEXT("ru");
	if (Culture.StartsWith(TEXT("tr"))) return TEXT("tr");
	if (Culture.StartsWith(TEXT("vi"))) return TEXT("vi");
	if (Culture.StartsWith(TEXT("th"))) return TEXT("th");
	if (Culture.StartsWith(TEXT("id"))) return TEXT("id");

	return TEXT("en");
}

FString FTapDBEventTracker::GetSystemLanguage() const
{
	// FPlatformMisc::GetDefaultLocale() returns the OS locale (e.g. "zh-Hans-CN", "en-US")
	// FInternationalization::GetCurrentCulture() returns the engine's culture, which defaults to "en"
	return FPlatformMisc::GetDefaultLocale();
}

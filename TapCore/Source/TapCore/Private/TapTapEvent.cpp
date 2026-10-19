// Copyright TapTap. All Rights Reserved.

#include "TapTapEvent.h"
#include "TapCoreModule.h"
#include "Internal/TapSdkChecker.h"
#include "Internal/TapDBInternal.h"
#include "Platform/ITapEventPlatform.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// Platform-specific headers
#if PLATFORM_ANDROID
	#include "Platform/TapEventAndroid.h"
#elif PLATFORM_IOS
	#include "Platform/TapEventiOS.h"
#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	#include "Platform/TapEventDesktop.h"
#endif

// =============================================================================
// Static Member Initialization
// =============================================================================

bool FTapTapEvent::bIsInitialized = false;
bool FTapTapEvent::bEnableLog = false;
TFunction<FString()> FTapTapEvent::DynamicPropertiesCallback = nullptr;
TUniquePtr<ITapEventPlatform> FTapTapEvent::PlatformImpl = nullptr;

// =============================================================================
// Platform Implementation Factory
// =============================================================================

void FTapTapEvent::CreatePlatformImpl()
{
	if (PlatformImpl.IsValid())
	{
		UE_LOG(LogTapCore, Warning, TEXT("[TapEvent] Platform implementation already exists"));
		return;
	}

	// Create platform-specific implementation based on compile-time platform
	#if PLATFORM_ANDROID
		PlatformImpl = MakeUnique<FTapEventAndroid>();
		UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Created Android platform implementation"));

	#elif PLATFORM_IOS
		PlatformImpl = MakeUnique<FTapEventiOS>();
		UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Created iOS platform implementation"));

	#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
		PlatformImpl = MakeUnique<FTapEventDesktop>();
		UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Created Desktop platform implementation"));

	#else
		#error "Unsupported platform for TapEvent"
	#endif

	if (!PlatformImpl.IsValid())
	{
		UE_LOG(LogTapCore, Error, TEXT("[TapEvent] Failed to create platform implementation"));
	}
}

// =============================================================================
// Public API Implementation
// =============================================================================

void FTapTapEvent::Init(bool bInEnableLog)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTapCore, Warning, TEXT("[TapEvent] Already initialized"));
		return;
	}

	bEnableLog = bInEnableLog;

	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Initialization Started"));
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] EnableLog: %s"), bEnableLog ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTapCore, Log, TEXT("========================================"));

	// Create platform implementation
	CreatePlatformImpl();

	if (!PlatformImpl.IsValid())
	{
		UE_LOG(LogTapCore, Error, TEXT("[TapEvent] Failed to create platform implementation"));
		return;
	}

	// Initialize platform-specific implementation
	PlatformImpl->Init(bEnableLog);

	bIsInitialized = true;

	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Initialized Successfully"));
	UE_LOG(LogTapCore, Log, TEXT("========================================"));
}

bool FTapTapEvent::CanUseEventApi()
{
	if (!FTapSdkChecker::CheckInitialize())
	{
		return false;
	}
	if (!FTapDBInternal::IsEnabled())
	{
		UE_LOG(LogTapCore, Warning, TEXT("[TapEvent] TapTapEvent is disabled; set FTapTapEventOptions.bEnableTapTapEvent = true"));
		return false;
	}
	if (!bIsInitialized || !PlatformImpl.IsValid())
	{
		UE_LOG(LogTapCore, Warning, TEXT("[TapEvent] Not initialized, call ignored"));
		return false;
	}
	return true;
}

void FTapTapEvent::SetUserID(const FString& UserID)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->SetUserID(UserID);
}

void FTapTapEvent::SetUserID(const FString& UserID, const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->SetUserID(UserID, Properties);
}

void FTapTapEvent::ClearUser()
{
	if (!CanUseEventApi()) return;
	PlatformImpl->ClearUser();
}

FString FTapTapEvent::GetDeviceId()
{
	if (!CanUseEventApi()) return TEXT("");
	return PlatformImpl->GetDeviceId();
}

void FTapTapEvent::LogEvent(const FString& Name, const FString& Properties)
{
	if (!CanUseEventApi()) return;
	if (Name.IsEmpty())
	{
		UE_LOG(LogTapCore, Error, TEXT("[TapEvent] LogEvent failed: event name is empty"));
		return;
	}
	PlatformImpl->LogEvent(Name, Properties);
}

void FTapTapEvent::DeviceInitialize(const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->DeviceInitialize(Properties);
}

void FTapTapEvent::DeviceUpdate(const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->DeviceUpdate(Properties);
}

void FTapTapEvent::DeviceAdd(const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->DeviceAdd(Properties);
}

void FTapTapEvent::UserInitialize(const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->UserInitialize(Properties);
}

void FTapTapEvent::UserUpdate(const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->UserUpdate(Properties);
}

void FTapTapEvent::UserAdd(const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->UserAdd(Properties);
}

void FTapTapEvent::AddCommonProperty(const FString& Key, const FString& Value)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->AddCommonProperty(Key, Value);
}

void FTapTapEvent::AddCommon(const FString& Properties)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->AddCommon(Properties);
}

void FTapTapEvent::ClearCommonProperty(const FString& Key)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->ClearCommonProperty(Key);
}

void FTapTapEvent::ClearCommonProperties(const TArray<FString>& Keys)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->ClearCommonProperties(Keys);
}

void FTapTapEvent::ClearAllCommonProperties()
{
	if (!CanUseEventApi()) return;
	PlatformImpl->ClearAllCommonProperties();
}

void FTapTapEvent::LogPurchasedEvent(
	const FString& OrderID,
	const FString& ProductName,
	int64 Amount,
	const FString& CurrencyType,
	const FString& PaymentMethod,
	const FString& Properties)
{
	if (!CanUseEventApi()) return;

	TSharedRef<FJsonObject> OrderObj = MakeShared<FJsonObject>();
	OrderObj->SetStringField(TEXT("orderId"), OrderID);
	OrderObj->SetStringField(TEXT("productName"), ProductName);
	OrderObj->SetNumberField(TEXT("amount"), static_cast<double>(Amount));
	OrderObj->SetStringField(TEXT("currencyType"), CurrencyType);
	OrderObj->SetStringField(TEXT("paymentMethod"), PaymentMethod);

	if (!Properties.IsEmpty())
	{
		TSharedPtr<FJsonObject> ExtraProps;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Properties);
		if (FJsonSerializer::Deserialize(Reader, ExtraProps) && ExtraProps.IsValid())
		{
			OrderObj->SetObjectField(TEXT("properties"), ExtraProps);
		}
		else
		{
			UE_LOG(LogTapCore, Warning, TEXT("[TapEvent] LogPurchasedEvent properties is not a JSON object; ignored"));
		}
	}

	FString OrderJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OrderJson);
	FJsonSerializer::Serialize(OrderObj, Writer);
	PlatformImpl->LogPurchasedEvent(OrderJson);
}

void FTapTapEvent::RegisterDynamicProperties(TFunction<FString()> Callback)
{
	if (!CanUseEventApi()) return;
	DynamicPropertiesCallback = Callback;
	PlatformImpl->RegisterDynamicProperties(Callback);
}

void FTapTapEvent::SetOAID(const FString& Value)
{
	if (!CanUseEventApi()) return;
	PlatformImpl->SetOAID(Value);
}

void FTapTapEvent::LogDeviceLoginEvent()
{
	if (!CanUseEventApi()) return;
	PlatformImpl->LogDeviceLoginEvent();
}

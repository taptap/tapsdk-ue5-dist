// Copyright TapTap. All Rights Reserved.

#include "Platform/TapEventDesktop.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapCoreModule.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/TapDBEventTracker.h"

FTapEventDesktop::FTapEventDesktop()
	: bIsInitialized(false)
	, bEnableLog(false)
	, DynamicPropertiesCallback(nullptr)
{
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Desktop implementation created"));
}

FTapEventDesktop::~FTapEventDesktop()
{
	// 析构时日志系统可能已被销毁（如点击窗口 X 退出），不能调用 UE_LOG
}

void FTapEventDesktop::Init(bool bInEnableLog)
{
	bEnableLog = bInEnableLog;

	const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
	const FTapTapEventOptions& EventOptions = FTapSdkConfigState::GetTapEventOptions(Config);

	FTapDBEventTracker::Get().Init(
		Config.ClientID,
		Config.ClientToken,
		Config.Region,
		bInEnableLog,
		Config.GameVersion,
		EventOptions.Channel,
		EventOptions.bDisableAutoLogDeviceLogin,
		EventOptions.bOverrideBuiltInParameters,
		EventOptions.PropertiesJson);

	bIsInitialized = true;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Desktop Init via TapDBEventTracker, EnableLog=%s, DisableAutoDeviceLogin=%s, OverrideBuiltIn=%s"),
		bEnableLog ? TEXT("true") : TEXT("false"),
		EventOptions.bDisableAutoLogDeviceLogin ? TEXT("true") : TEXT("false"),
		EventOptions.bOverrideBuiltInParameters ? TEXT("true") : TEXT("false"));
}

void FTapEventDesktop::SetUserID(const FString& UserID)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().SetUserID(UserID);
}

void FTapEventDesktop::SetUserID(const FString& UserID, const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().SetUserID(UserID, Properties);
}

void FTapEventDesktop::ClearUser()
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().ClearUser();
}

FString FTapEventDesktop::GetDeviceId()
{
	if (!bIsInitialized) return TEXT("");
	return FTapDBEventTracker::Get().GetDeviceId();
}

void FTapEventDesktop::LogEvent(const FString& Name, const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().LogEvent(Name, Properties);
}

void FTapEventDesktop::DeviceInitialize(const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().DeviceInitialize(Properties);
}

void FTapEventDesktop::DeviceUpdate(const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().DeviceUpdate(Properties);
}

void FTapEventDesktop::DeviceAdd(const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().DeviceAdd(Properties);
}

void FTapEventDesktop::UserInitialize(const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().UserInitialize(Properties);
}

void FTapEventDesktop::UserUpdate(const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().UserUpdate(Properties);
}

void FTapEventDesktop::UserAdd(const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().UserAdd(Properties);
}

void FTapEventDesktop::AddCommonProperty(const FString& Key, const FString& Value)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().AddCommonProperty(Key, Value);
}

void FTapEventDesktop::AddCommon(const FString& Properties)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().AddCommon(Properties);
}

void FTapEventDesktop::ClearCommonProperty(const FString& Key)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().ClearCommonProperty(Key);
}

void FTapEventDesktop::ClearCommonProperties(const TArray<FString>& Keys)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().ClearCommonProperties(Keys);
}

void FTapEventDesktop::ClearAllCommonProperties()
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().ClearAllCommonProperties();
}

void FTapEventDesktop::LogPurchasedEvent(const FString& OrderJson)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().LogPurchasedEvent(OrderJson);
}

void FTapEventDesktop::RegisterDynamicProperties(TFunction<FString()> Callback)
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().RegisterDynamicProperties(Callback);
}

void FTapEventDesktop::SetOAID(const FString& Value)
{
	// Desktop does not use OAID
	if (bEnableLog)
	{
		UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Desktop SetOAID (not used on Desktop): %s"), *Value);
	}
}

void FTapEventDesktop::LogDeviceLoginEvent()
{
	if (!bIsInitialized) return;
	FTapDBEventTracker::Get().LogDeviceLoginEvent();
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

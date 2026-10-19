// Copyright TapTap. All Rights Reserved.

#include "Platform/TapEventiOS.h"

#if PLATFORM_IOS

#include "TapCoreModule.h"

// Suppress Objective-C warnings
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-property-no-attribute"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wnonnull"

// Import Pure Objective-C bridge (same as TapCoreiOS.mm)
#import "TapCoreIOSBridge.h"

#pragma clang diagnostic pop

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapEventiOS::FTapEventiOS()
	: bIsInitialized(false)
	, DynamicPropertiesCallback(nullptr)
{
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS implementation created"));
}

FTapEventiOS::~FTapEventiOS()
{
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS implementation destroyed"));
}

// =============================================================================
// ITapEventPlatform Interface Implementation
// =============================================================================

void FTapEventiOS::Init(bool bEnableLog)
{
	bIsInitialized = true;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS Init, EnableLog=%s"), bEnableLog ? TEXT("true") : TEXT("false"));
	// TapDB 的初始化由 TapCore 统一处理（TapCoreIOSBridge initWithConfig），这里只记录状态
}

void FTapEventiOS::SetUserID(const FString& UserID)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS SetUserID: %s"), *UserID);
	[TapCoreIOSBridge eventSetUserID:UserID.GetNSString()];
}

void FTapEventiOS::SetUserID(const FString& UserID, const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS SetUserID: %s, Properties: %s"), *UserID, *Properties);
	[TapCoreIOSBridge eventSetUserID:UserID.GetNSString() properties:Properties.GetNSString()];
}

void FTapEventiOS::ClearUser()
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS ClearUser"));
	[TapCoreIOSBridge eventClearUser];
}

FString FTapEventiOS::GetDeviceId()
{
	if (!bIsInitialized) return TEXT("");
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS GetDeviceId"));
	NSString* DeviceId = [TapCoreIOSBridge eventGetDeviceId];
	return FString(DeviceId);
}

void FTapEventiOS::LogEvent(const FString& Name, const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS LogEvent: %s"), *Name);
	[TapCoreIOSBridge eventLogEvent:Name.GetNSString() properties:Properties.GetNSString()];
}

void FTapEventiOS::DeviceInitialize(const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS DeviceInitialize"));
	[TapCoreIOSBridge eventDeviceInitialize:Properties.GetNSString()];
}

void FTapEventiOS::DeviceUpdate(const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS DeviceUpdate"));
	[TapCoreIOSBridge eventDeviceUpdate:Properties.GetNSString()];
}

void FTapEventiOS::DeviceAdd(const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS DeviceAdd"));
	[TapCoreIOSBridge eventDeviceAdd:Properties.GetNSString()];
}

void FTapEventiOS::UserInitialize(const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS UserInitialize"));
	[TapCoreIOSBridge eventUserInitialize:Properties.GetNSString()];
}

void FTapEventiOS::UserUpdate(const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS UserUpdate"));
	[TapCoreIOSBridge eventUserUpdate:Properties.GetNSString()];
}

void FTapEventiOS::UserAdd(const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS UserAdd"));
	[TapCoreIOSBridge eventUserAdd:Properties.GetNSString()];
}

void FTapEventiOS::AddCommonProperty(const FString& Key, const FString& Value)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS AddCommonProperty: %s = %s"), *Key, *Value);
	[TapCoreIOSBridge eventAddCommonProperty:Key.GetNSString() value:Value.GetNSString()];
}

void FTapEventiOS::AddCommon(const FString& Properties)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS AddCommon"));
	[TapCoreIOSBridge eventAddCommon:Properties.GetNSString()];
}

void FTapEventiOS::ClearCommonProperty(const FString& Key)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS ClearCommonProperty: %s"), *Key);
	[TapCoreIOSBridge eventClearCommonProperty:Key.GetNSString()];
}

void FTapEventiOS::ClearCommonProperties(const TArray<FString>& Keys)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS ClearCommonProperties: %d keys"), Keys.Num());

	NSMutableArray<NSString*>* NSKeys = [NSMutableArray arrayWithCapacity:Keys.Num()];
	for (const FString& Key : Keys)
	{
		[NSKeys addObject:Key.GetNSString()];
	}
	[TapCoreIOSBridge eventClearCommonProperties:NSKeys];
}

void FTapEventiOS::ClearAllCommonProperties()
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS ClearAllCommonProperties"));
	[TapCoreIOSBridge eventClearAllCommonProperties];
}

void FTapEventiOS::LogPurchasedEvent(const FString& OrderJson)
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS LogPurchasedEvent: %s"), *OrderJson);
	[TapCoreIOSBridge eventLogPurchasedEvent:OrderJson.GetNSString()];
}

// =============================================================================
// Dynamic Properties
// =============================================================================

void FTapEventiOS::RegisterDynamicProperties(TFunction<FString()> Callback)
{
	if (!bIsInitialized) return;

	DynamicPropertiesCallback = Callback;

	// 捕获成员变量指针，在 ObjC block 中调用 C++ TFunction
	TFunction<FString()>* CallbackPtr = &DynamicPropertiesCallback;

	[TapCoreIOSBridge eventRegisterDynamicProperties:^NSDictionary* {
		if (CallbackPtr && *CallbackPtr)
		{
			FString Result = (*CallbackPtr)();
			NSString* JsonStr = Result.GetNSString();
			NSDictionary* Dict = [TapCoreIOSBridge parseJson:JsonStr];
			return Dict ?: @{};
		}
		return @{};
	}];

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS RegisterDynamicProperties registered via bridge"));
}

void FTapEventiOS::SetOAID(const FString& Value)
{
	if (!bIsInitialized) return;
	// iOS 不使用 OAID
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS SetOAID (not used on iOS)"));
}

void FTapEventiOS::LogDeviceLoginEvent()
{
	if (!bIsInitialized) return;
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] iOS LogDeviceLoginEvent"));
	[TapCoreIOSBridge eventLogDeviceLoginEvent];
}

#endif // PLATFORM_IOS

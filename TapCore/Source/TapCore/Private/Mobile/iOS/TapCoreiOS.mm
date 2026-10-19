// Copyright TapTap. All Rights Reserved.

#include "Platform/TapCoreiOS.h"

#if PLATFORM_IOS

#include "TapCore.h"
#include "Internal/TapCoreInternal.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/TapSdkOptionSerializerRegistry.h"
#include "Async/Async.h"
#include "IOS/IOSAppDelegate.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// Suppress Objective-C warnings
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-property-no-attribute"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wnonnull"

// Import Pure Objective-C bridge
#import "TapCoreIOSBridge.h"

// Note: iOS SDK headers are imported in TapCoreIOSBridge.m
// #import <TapTapSDK/TapTapSDK.h> is already in TapCoreIOSBridge.m

#pragma clang diagnostic pop

DEFINE_LOG_CATEGORY_STATIC(LogTapiOS, Log, All);

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapCoreiOS::FTapCoreiOS()
	: bIsInitialized(false)
{
	UE_LOG(LogTapiOS, Log, TEXT("TapCore iOS implementation created"));
}

FTapCoreiOS::~FTapCoreiOS()
{
	UE_LOG(LogTapiOS, Log, TEXT("TapCore iOS implementation destroyed"));
}

// =============================================================================
// ITapCorePlatform Interface Implementation
// =============================================================================

void FTapCoreiOS::Init(const FTapTapSdkOptions& Config)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTapiOS, Warning, TEXT("iOS platform already initialized"));
		return;
	}

	UE_LOG(LogTapiOS, Log, TEXT("Initializing TapCore iOS with ClientID: %s"), *Config.ClientID);

	// Store config
	CurrentConfig = Config;

	// Call iOS Native SDK on main thread
	CalliOSInit(Config);


	UE_LOG(LogTapiOS, Log, TEXT("TapCore iOS init request sent"));
}

FString FTapCoreiOS::GetPlatformVersion() const
{
	// Get version from iOS SDK via bridge
	NSString* version = [TapCoreIOSBridge getSDKVersion];
	return FString(version);
}

bool FTapCoreiOS::IsInitialized() const
{
	return bIsInitialized;
}

FString FTapCoreiOS::GetPlatformName() const
{
	return TEXT("iOS");
}

void FTapCoreiOS::UpdateLanguage(ETapLanguage Language)
{
	UE_LOG(LogTapiOS, Log, TEXT("Updating language to: %d"), static_cast<int32>(Language));

	// Call iOS SDK on main thread
	dispatch_async(dispatch_get_main_queue(), ^{
		[TapCoreIOSBridge updateLanguage:static_cast<int32>(Language)];
	});

	UE_LOG(LogTapiOS, Log, TEXT("Language update request sent to iOS SDK"));
}

// =============================================================================
// Private Methods
// =============================================================================

void FTapCoreiOS::CalliOSInit(const FTapTapSdkOptions& Config)
{
	// Convert FTapTapSdkOptions to JSON string
	FString ConfigJson = ConvertConfigToJson(Config);

	const uint64 AttemptId = FTapCoreInternal::GetInitAttemptId();

	// Capture screen orientation value for use inside ObjC block
	int32 ScreenOrientation = Config.ScreenOrientation;

	// iOS SDK calls must be made on the main thread
	dispatch_async(dispatch_get_main_queue(), ^{
		if (!FTapCoreInternal::IsInitAttemptActive(AttemptId)) return;

		UE_LOG(LogTapiOS, Log, TEXT("Calling iOS SDK init on main thread"));

		// Convert FString to NSString
		NSString* nsConfigJson = ConfigJson.GetNSString();

		// Call Pure Objective-C bridge
		[TapCoreIOSBridge setPlatform:@"TapSDK-UE" version:FTapTapSDK::GetVersion().GetNSString()];
		[TapCoreIOSBridge initWithConfig:nsConfigJson completion:^(BOOL success, NSString* errorMsg) {
			if (FTapCoreInternal::GetInitAttemptId() != AttemptId) return;

			// Apply screen orientation after SDK is initialized
			if (success) {
				BOOL isLandscape = (ScreenOrientation == 1);
				UE_LOG(LogTapiOS, Log, TEXT("Applying screen orientation: %s"),
					isLandscape ? TEXT("Landscape") : TEXT("Portrait"));

				if (@available(iOS 16.0, *)) {
					// iOS 16+: use UIWindowScene requestGeometryUpdateWithPreferences
					UIWindowScene* windowScene = nil;
					for (UIScene* scene in [UIApplication sharedApplication].connectedScenes) {
						if ([scene isKindOfClass:[UIWindowScene class]]) {
							windowScene = (UIWindowScene*)scene;
							break;
						}
					}
					if (windowScene) {
						UIInterfaceOrientationMask mask = isLandscape
							? UIInterfaceOrientationMaskLandscape
							: UIInterfaceOrientationMaskPortrait;
						UIWindowSceneGeometryPreferencesIOS* prefs =
							[[UIWindowSceneGeometryPreferencesIOS alloc] initWithInterfaceOrientations:mask];
						[windowScene requestGeometryUpdateWithPreferences:prefs
							errorHandler:^(NSError* error) {
								NSLog(@"[TapCoreiOS] Failed to update orientation: %@", error.localizedDescription);
							}];
					}
					// Notify root VC to pick up the new supported orientations
					UIWindow* keyWindow = windowScene ? windowScene.windows.firstObject : nil;
					[keyWindow.rootViewController setNeedsUpdateOfSupportedInterfaceOrientations];
				} else {
					// iOS 15: force via UIDevice private KVC (widely-used approach)
					UIInterfaceOrientation targetOrientation = isLandscape
						? UIInterfaceOrientationLandscapeLeft
						: UIInterfaceOrientationPortrait;
					[[UIDevice currentDevice] setValue:@(targetOrientation) forKey:@"orientation"];
					[UIViewController attemptRotationToDeviceOrientation];
				}
			}

			// Switch back to game thread for callback
			FString ErrorMessage = errorMsg ? FString(errorMsg) : FString();

			// Acceptance does not complete initialization. Only report immediate rejection here.
			if (!success)
			{
				AsyncTask(ENamedThreads::GameThread, [AttemptId, ErrorMessage]() {
					FTapCoreInternal::NotifyInitFailed(1003, ErrorMessage, AttemptId);
				});
			}
		} finalResult:^(NSInteger Code, NSString* Message) {
			const FString ErrorMessage = Message ? FString(Message) : FString();
			AsyncTask(ENamedThreads::GameThread, [AttemptId, Code, ErrorMessage]() {
				FTapCoreInternal::NotifyInitResult(static_cast<int32>(Code), ErrorMessage, AttemptId);
			});
		}];
	});
}

FString FTapCoreiOS::ConvertConfigToJson(const FTapTapSdkOptions& Config) const
{
	const FTapTapEventOptions& EventOptions = FTapSdkConfigState::GetTapEventOptions(Config);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	JsonObject->SetStringField(TEXT("clientId"), Config.ClientID);
	JsonObject->SetStringField(TEXT("clientToken"), Config.ClientToken);
	JsonObject->SetNumberField(TEXT("region"), static_cast<int32>(Config.Region));
	JsonObject->SetNumberField(TEXT("preferredLanguage"), static_cast<int32>(Config.PreferredLanguage));
	JsonObject->SetBoolField(TEXT("enableLog"), Config.bEnableLog);
	JsonObject->SetBoolField(TEXT("enableTapDB"), EventOptions.bEnableTapTapEvent);
	JsonObject->SetBoolField(TEXT("autoIAPEventEnabled"), EventOptions.bEnableAutoIAPEvent);
	JsonObject->SetBoolField(TEXT("overrideBuiltInParameters"), EventOptions.bOverrideBuiltInParameters);
	JsonObject->SetBoolField(TEXT("disableAutoLogDeviceLogin"), EventOptions.bDisableAutoLogDeviceLogin);
	JsonObject->SetBoolField(TEXT("enableAdvertiserIDCollection"), EventOptions.bEnableAdvertiserIDCollection);
	JsonObject->SetStringField(TEXT("tapsdk_artifact"), TEXT("UE5"));
	JsonObject->SetNumberField(TEXT("screenOrientation"), Config.ScreenOrientation);
	JsonObject->SetBoolField(TEXT("isRND"), FTapSdkConfigState::IsRNDEnvironment());

	TArray<FTapSerializedSdkModuleOption> SerializedOptions;
	FTapSdkOptionSerializerRegistry::Get().SerializeOptions(Config, SerializedOptions);
	TArray<TSharedPtr<FJsonValue>> OtherOptionsJson;
	for (const FTapSerializedSdkModuleOption& SerializedOption : SerializedOptions)
	{
		const TSharedRef<FJsonObject> Descriptor = MakeShared<FJsonObject>();
		Descriptor->SetStringField(TEXT("nativeClass"), SerializedOption.NativeClassName);
		Descriptor->SetStringField(TEXT("json"), SerializedOption.Json);
		OtherOptionsJson.Add(MakeShared<FJsonValueObject>(Descriptor));
	}
	JsonObject->SetArrayField(TEXT("otherOptions"), OtherOptionsJson);
	if (!EventOptions.Channel.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("channel"), EventOptions.Channel);
	}

	if (!Config.GameVersion.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("gameVersion"), Config.GameVersion);
	}

	if (!EventOptions.CAID.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("caid"), EventOptions.CAID);
	}

	if (!EventOptions.PropertiesJson.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("propertiesJson"), EventOptions.PropertiesJson);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

void* FTapCoreiOS::CreateiOSConfig(const FTapTapSdkOptions& Config) const
{
	// TODO: Implement when iOS SDK is available
	/*
	TapCoreConfig* config = [[TapCoreConfig alloc] init];
	config.clientId = Config.ClientID.GetNSString();
	config.clientToken = Config.ClientToken.GetNSString();
	// ... set other properties
	return (__bridge_retained void*)config;
	*/

	return nullptr;
}

void FTapCoreiOS::RequestGatekeeperValidation(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTemp, Log, TEXT("Gatekeeper validation handled by iOS Native SDK (tap-core)"));
	// The iOS tap-core SDK handles gatekeeper automatically
	// No explicit call needed from UE side
}

#endif // PLATFORM_IOS

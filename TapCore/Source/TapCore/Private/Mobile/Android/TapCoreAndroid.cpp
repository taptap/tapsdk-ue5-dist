// Copyright TapTap. All Rights Reserved.

#include "Internal/Platform/TapCoreAndroid.h"
#include "Internal/TapLogPrivacy.h"
#include "Internal/TapAndroidCallbackId.h"

#if PLATFORM_ANDROID

#include "TapCore.h"
#include "Internal/TapCoreInternal.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/TapSdkOptionSerializerRegistry.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Async/Async.h"
#include "Misc/ConfigCacheIni.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Platform/TapCoreRouterMethods.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapAndroid, Log, All);

namespace
{
	constexpr const TCHAR* SetPlatformAndVersionMethod = TEXT("setPlatformAndVersion");
	constexpr const TCHAR* SetSDKArtifactMethod = TEXT("setSDKArtifact");

	void CallInternalRouteWithJson(const FString& MethodName, const TSharedRef<FJsonObject>& JsonObject)
	{
		FString ParamsJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsJson);
		FJsonSerializer::Serialize(JsonObject, Writer);

		FString Result = FTapCoreAndroid::CallModuleRoute(MethodName, ParamsJson);
		UE_LOG(LogTapAndroid, Verbose, TEXT("Internal route result for %s: %s"), *MethodName, *Result);
	}

	void SetPlatformAndVersionInternal(const FString& Platform, const FString& Version)
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		JsonObject->SetStringField(TEXT("platform"), Platform);
		JsonObject->SetStringField(TEXT("version"), Version);
		CallInternalRouteWithJson(SetPlatformAndVersionMethod, JsonObject);
	}

	void SetSDKArtifactInternal(const FString& Artifact)
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		JsonObject->SetStringField(TEXT("artifact"), Artifact);
		CallInternalRouteWithJson(SetSDKArtifactMethod, JsonObject);
	}
}

// =============================================================================
// Static Members
// =============================================================================

TMap<int64, TFunction<void(const FString&)>> FTapCoreAndroid::CallbackRegistry;
FCriticalSection FTapCoreAndroid::CallbackLock;
TArray<TFunction<bool(int64, const FString&)>> FTapCoreAndroid::GlobalCallbackHandlers;

// =============================================================================
// JNI Native Method: nativeOnResult
// =============================================================================

/**
 * JNI native method implementation
 * Called from Java: EngineBridgeCallbackProxy.nativeOnResult(long, String)
 */
extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_kit_internal_enginebridge_EngineBridgeCallbackProxy_nativeOnResult(
	JNIEnv* Env,
	jobject Obj,
	jlong NativeHandle,
	jstring JsonString)
{
	if (Env == nullptr || JsonString == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("nativeOnResult: Invalid parameters"));
		return;
	}

	// Convert JSON string
	const char* JsonChars = Env->GetStringUTFChars(JsonString, nullptr);
	FString ResultJson(UTF8_TO_TCHAR(JsonChars));
	Env->ReleaseStringUTFChars(JsonString, JsonChars);

	UE_LOG(LogTapAndroid, Log, TEXT("nativeOnResult called with handle=%lld, json=%s"),
		NativeHandle, *ResultJson);

	// Dispatch callback to all registered modules (TapCore, TapLogin, etc.)
	FTapCoreAndroid::DispatchGlobalCallback(static_cast<int64>(NativeHandle), ResultJson);
}

// The Activity owns a one-shot native init listener for each UE attempt.
extern "C" JNIEXPORT void JNICALL
Java_com_epicgames_unreal_GameActivity_nativeOnTapCoreInitResult(
	JNIEnv* Env, jobject Activity, jlong Attempt, jint Code, jstring Message)
{
	FString ErrorMessage;
	if (Message)
	{
		const char* Chars = Env->GetStringUTFChars(Message, nullptr);
		if (Chars)
		{
			ErrorMessage = UTF8_TO_TCHAR(Chars);
			Env->ReleaseStringUTFChars(Message, Chars);
		}
	}
	AsyncTask(ENamedThreads::GameThread, [Attempt, Code, ErrorMessage]()
	{
		FTapCoreInternal::NotifyInitResult(Code, ErrorMessage, static_cast<uint64>(Attempt));
	});
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapCoreAndroid::FTapCoreAndroid()
	: bIsInitialized(false)
{
	UE_LOG(LogTapAndroid, Log, TEXT("TapCore Android implementation created"));
	InitializeJNI();
}

FTapCoreAndroid::~FTapCoreAndroid()
{
	UE_LOG(LogTapAndroid, Log, TEXT("TapCore Android implementation destroyed"));
}

// =============================================================================
// ITapCorePlatform Interface Implementation
// =============================================================================

void FTapCoreAndroid::Init(const FTapTapSdkOptions& Config)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTapAndroid, Warning, TEXT("Android platform already initialized"));
		return;
	}

	// Ensure we're on the game thread
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [this, Config]()
		{
			Init(Config);
		});
		return;
	}

	UE_LOG(LogTapAndroid, Log, TEXT("Initializing TapCore Android with ClientID: %s"), *Config.ClientID);

	CallAndroidInit(Config);

	// Apply screen orientation to the Android app
	ApplyScreenOrientation(Config.ScreenOrientation);

	UE_LOG(LogTapAndroid, Log, TEXT("TapCore Android initialization requested"));
}

FString FTapCoreAndroid::GetPlatformVersion() const
{
	// TODO: Get version from Android SDK via Router
	return TEXT("1.0.0-android");
}

bool FTapCoreAndroid::IsInitialized() const
{
	// Query actual init status from Android SDK
	FString Result = CallModuleRoute(TapCore::RouterMethods::IS_INITIALIZED);

	FString Data;
	FString Message;
	if (ParseRouteResponse(Result, Data, Message))
	{
		return Data.Equals(TEXT("true"), ESearchCase::IgnoreCase);
	}

	return bIsInitialized;
}

FString FTapCoreAndroid::GetPlatformName() const
{
	return TEXT("Android");
}

void FTapCoreAndroid::UpdateLanguage(ETapLanguage Language)
{
	FString LanguageCode = FString::FromInt(static_cast<int32>(Language));
	FString Result = CallModuleRoute(TapCore::RouterMethods::UPDATE_LANGUAGE, LanguageCode);

	FString Data;
	FString Message;
	if (!ParseRouteResponse(Result, Data, Message))
	{
		UE_LOG(LogTapAndroid, Warning, TEXT("Failed to update language: %s"), *Message);
	}
	else
	{
		UE_LOG(LogTapAndroid, Log, TEXT("Language updated successfully"));
	}
}

// =============================================================================
// Private Methods
// =============================================================================

void FTapCoreAndroid::InitializeJNI()
{
	UE_LOG(LogTapAndroid, Log, TEXT("Initializing JNI environment"));

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to get JNI environment"));
		return;
	}

	UE_LOG(LogTapAndroid, Log, TEXT("JNI environment initialized successfully"));
}

void FTapCoreAndroid::CallAndroidInit(const FTapTapSdkOptions& Config)
{
	const uint64 AttemptId = FTapCoreInternal::GetInitAttemptId();
	SetPlatformAndVersionInternal(TEXT("TapSDK-UE"), Config.GameVersion.IsEmpty() ? TEXT("unknown") : Config.GameVersion);
	SetSDKArtifactInternal(TEXT("UE5"));

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	jobject Activity = FAndroidApplication::GetGameActivityThis();
	if (!Env || !Activity)
	{
		FTapCoreInternal::NotifyInitFailed(1003, TEXT("Android JNI environment or Activity unavailable"), AttemptId);
		return;
	}
	jclass ActivityClass = Env->GetObjectClass(Activity);
	jmethodID Method = ActivityClass ? Env->GetMethodID(ActivityClass, "initTapCore", "(Ljava/lang/String;J)V") : nullptr;
	jstring Json = nullptr;
	if (Method && !Env->ExceptionCheck())
	{
		Json = Env->NewStringUTF(TCHAR_TO_UTF8(*ConfigToJson(Config)));
		if (Json && !Env->ExceptionCheck())
		{
			Env->CallVoidMethod(Activity, Method, Json, static_cast<jlong>(AttemptId));
		}
	}
	const bool bFailed = !Method || !Json || Env->ExceptionCheck();
	if (Env->ExceptionCheck())
	{
		Env->ExceptionDescribe();
		Env->ExceptionClear();
	}
	if (Json) Env->DeleteLocalRef(Json);
	if (ActivityClass) Env->DeleteLocalRef(ActivityClass);
	if (bFailed)
	{
		FTapCoreInternal::NotifyInitFailed(1003, TEXT("Failed to invoke Android initialization"), AttemptId);
	}
}

FString FTapCoreAndroid::ConfigToJson(const FTapTapSdkOptions& Config) const
{
	const FTapTapEventOptions& EventOptions = FTapSdkConfigState::GetTapEventOptions(Config);
	TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);

	JsonObject->SetStringField(TEXT("clientId"), Config.ClientID);
	JsonObject->SetStringField(TEXT("clientToken"), Config.ClientToken);

	// Region mapping: CN=0, Global=1, etc.
	JsonObject->SetNumberField(TEXT("region"), static_cast<int32>(Config.Region));

	// Language mapping
	JsonObject->SetNumberField(TEXT("preferredLanguage"), static_cast<int32>(Config.PreferredLanguage));

	JsonObject->SetBoolField(TEXT("enableLog"), Config.bEnableLog);

	JsonObject->SetBoolField(TEXT("enableTapDB"), EventOptions.bEnableTapTapEvent);
	JsonObject->SetBoolField(TEXT("autoIAPEventEnabled"), EventOptions.bEnableAutoIAPEvent);
	JsonObject->SetBoolField(TEXT("overrideBuiltInParameters"), EventOptions.bOverrideBuiltInParameters);
	JsonObject->SetBoolField(TEXT("disableAutoLogDeviceLogin"), EventOptions.bDisableAutoLogDeviceLogin);
	JsonObject->SetBoolField(TEXT("disableReflectionOAID"), EventOptions.bDisableReflectionOAID);
	if (!EventOptions.Channel.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("channel"), EventOptions.Channel);
	}
	if (!EventOptions.CAID.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("caid"), EventOptions.CAID);
	}
	if (!EventOptions.OAIDCert.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("oaidCert"), EventOptions.OAIDCert);
	}

	if (!Config.GameVersion.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("gameVersion"), Config.GameVersion);
	}

	if (!EventOptions.PropertiesJson.IsEmpty())
	{
		JsonObject->SetStringField(TEXT("properties"), EventOptions.PropertiesJson);
	}

	// Add RND environment flag
	JsonObject->SetBoolField(TEXT("isRND"), FTapSdkConfigState::IsRNDEnvironment());

	// Add screen orientation
	JsonObject->SetNumberField(TEXT("screenOrientation"), Config.ScreenOrientation);

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

	// Set artifact to UE5 for engine identification
	JsonObject->SetStringField(TEXT("tapsdk_artifact"), TEXT("UE5"));

	// Convert to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer);

	return OutputString;
}

// =============================================================================
// Router Methods - Static
// =============================================================================

FString FTapCoreAndroid::CallModuleRoute(const FString& MethodName, const FString& ParamsJson)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("JNI environment is null"));
		return TEXT("{\"success\":false,\"message\":\"JNI environment is null\"}");
	}

	// Create request JSON
	FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);

	UE_LOG(LogTapAndroid, Verbose, TEXT("CallModuleRoute: %s"), *TapLogPrivacy::Body(RequestJson));

	// Get GameActivity
	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (GameActivity == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("GameActivity is null"));
		return TEXT("{\"success\":false,\"message\":\"GameActivity is null\"}");
	}

	// Get Activity class
	jclass ActivityClass = Env->GetObjectClass(GameActivity);
	if (ActivityClass == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to get Activity class"));
		return TEXT("{\"success\":false,\"message\":\"Failed to get Activity class\"}");
	}

	// Get routeToTapCore method
	jmethodID RouteMethod = Env->GetMethodID(
		ActivityClass,
		"routeToTapCore",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
	);

	if (RouteMethod == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to find routeToTapCore method"));
		Env->DeleteLocalRef(ActivityClass);
		return TEXT("{\"success\":false,\"message\":\"Failed to find routeToTapCore method\"}");
	}

	// Create Java string
	jstring jRequestJson = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));

	// Call method (sync, callback = null)
	jstring jResult = (jstring)Env->CallObjectMethod(
		GameActivity,
		RouteMethod,
		jRequestJson,
		nullptr  // Sync call, callback is null
	);

	// Check for exceptions
	if (Env->ExceptionCheck())
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Exception occurred during routeToTapCore call"));
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		Env->DeleteLocalRef(jRequestJson);
		Env->DeleteLocalRef(ActivityClass);
		return TEXT("{\"success\":false,\"message\":\"JNI exception occurred\"}");
	}

	// Convert result
	FString Result;
	if (jResult != nullptr)
	{
		const char* ResultChars = Env->GetStringUTFChars(jResult, nullptr);
		Result = FString(UTF8_TO_TCHAR(ResultChars));
		Env->ReleaseStringUTFChars(jResult, ResultChars);
		Env->DeleteLocalRef(jResult);
	}
	else
	{
		Result = TEXT("{\"success\":false,\"message\":\"Result is null\"}");
	}

	// Clean up
	Env->DeleteLocalRef(jRequestJson);
	Env->DeleteLocalRef(ActivityClass);

	UE_LOG(LogTapAndroid, Verbose, TEXT("CallModuleRoute result: %s"), *Result);

	return Result;
}

void FTapCoreAndroid::CallModuleRouteAsync(
	const FString& MethodName,
	const FString& ParamsJson,
	TFunction<void(const FString&)> Callback)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("JNI environment is null"));
		AsyncTask(ENamedThreads::GameThread, [Callback]()
		{
			Callback(TEXT("{\"success\":false,\"message\":\"JNI environment is null\"}"));
		});
		return;
	}

	// Create request JSON
	FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);

	UE_LOG(LogTapAndroid, Verbose, TEXT("CallModuleRouteAsync: %s"), *TapLogPrivacy::Body(RequestJson));

	// Create callback object
	jobject CallbackObj = CreateEngineBridgeCallback(Env, Callback);
	if (CallbackObj == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to create callback object"));
		AsyncTask(ENamedThreads::GameThread, [Callback]()
		{
			Callback(TEXT("{\"success\":false,\"message\":\"Failed to create callback object\"}"));
		});
		return;
	}

	// Get GameActivity
	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (GameActivity == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("GameActivity is null"));
		DestroyEngineBridgeCallback(Env, CallbackObj);
		AsyncTask(ENamedThreads::GameThread, [Callback]()
		{
			Callback(TEXT("{\"success\":false,\"message\":\"GameActivity is null\"}"));
		});
		return;
	}

	// Get Activity class
	jclass ActivityClass = Env->GetObjectClass(GameActivity);
	if (ActivityClass == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to get Activity class"));
		DestroyEngineBridgeCallback(Env, CallbackObj);
		return;
	}

	// Get routeToTapCore method
	jmethodID RouteMethod = Env->GetMethodID(
		ActivityClass,
		"routeToTapCore",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
	);

	if (RouteMethod == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to find routeToTapCore method"));
		Env->DeleteLocalRef(ActivityClass);
		DestroyEngineBridgeCallback(Env, CallbackObj);
		return;
	}

	// Create Java string
	jstring jRequestJson = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));

	// Call method (async, with callback)
	jstring jResult = (jstring)Env->CallObjectMethod(
		GameActivity,
		RouteMethod,
		jRequestJson,
		CallbackObj  // Async call, pass callback object
	);

	// Check for exceptions
	if (Env->ExceptionCheck())
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Exception occurred during async routeToTapCore call"));
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		DestroyEngineBridgeCallback(Env, CallbackObj);
	}

	// Log immediate response (should be {"success":true,"async":true})
	if (jResult != nullptr)
	{
		const char* ResultChars = Env->GetStringUTFChars(jResult, nullptr);
		FString Result(UTF8_TO_TCHAR(ResultChars));
		Env->ReleaseStringUTFChars(jResult, ResultChars);
		Env->DeleteLocalRef(jResult);
		UE_LOG(LogTapAndroid, Verbose, TEXT("CallModuleRouteAsync immediate result: %s"), *Result);
	}

	// Clean up (callback will be cleaned up when onResult is called)
	Env->DeleteLocalRef(jRequestJson);
	Env->DeleteLocalRef(ActivityClass);
}

// =============================================================================
// JNI Callback Management - Static
// =============================================================================

jobject FTapCoreAndroid::CreateEngineBridgeCallback(JNIEnv* Env, TFunction<void(const FString&)> Callback)
{
	// Generate unique callback ID
	int64 CallbackId;
	{
		FScopeLock Lock(&CallbackLock);
		CallbackId = FTapAndroidCallbackId::Allocate();
		CallbackRegistry.Add(CallbackId, Callback);
	}

	UE_LOG(LogTapAndroid, Verbose, TEXT("CreateEngineBridgeCallback: ID=%lld"), CallbackId);

	// Find EngineBridgeCallbackProxy class
	jclass ProxyClass = FAndroidApplication::FindJavaClass(
		"com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy"
	);

	if (ProxyClass == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to find EngineBridgeCallbackProxy class"));
		FScopeLock Lock(&CallbackLock);
		CallbackRegistry.Remove(CallbackId);
		return nullptr;
	}

	// Get constructor: EngineBridgeCallbackProxy(long)
	jmethodID Constructor = Env->GetMethodID(ProxyClass, "<init>", "(J)V");
	if (Constructor == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to find EngineBridgeCallbackProxy constructor"));
		Env->DeleteLocalRef(ProxyClass);
		FScopeLock Lock(&CallbackLock);
		CallbackRegistry.Remove(CallbackId);
		return nullptr;
	}

	// Create object instance
	jobject ProxyObj = Env->NewObject(ProxyClass, Constructor, (jlong)CallbackId);
	if (ProxyObj == nullptr)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to create EngineBridgeCallbackProxy instance"));
		Env->DeleteLocalRef(ProxyClass);
		FScopeLock Lock(&CallbackLock);
		CallbackRegistry.Remove(CallbackId);
		return nullptr;
	}

	// Create global reference (prevent GC)
	jobject GlobalProxyObj = Env->NewGlobalRef(ProxyObj);

	// Clean up local references
	Env->DeleteLocalRef(ProxyObj);
	Env->DeleteLocalRef(ProxyClass);

	return GlobalProxyObj;
}

void FTapCoreAndroid::DestroyEngineBridgeCallback(JNIEnv* Env, jobject CallbackObj)
{
	if (CallbackObj != nullptr && Env != nullptr)
	{
		Env->DeleteGlobalRef(CallbackObj);
	}
}

void FTapCoreAndroid::RegisterCallback(int64 CallbackId, TFunction<void(const FString&)> Callback)
{
	FScopeLock Lock(&CallbackLock);
	CallbackRegistry.Add(CallbackId, Callback);
	UE_LOG(LogTapAndroid, Verbose, TEXT("RegisterCallback: ID=%lld"), CallbackId);
}

void FTapCoreAndroid::TriggerCallback(int64 CallbackId, const FString& ResultJson)
{
	TFunction<void(const FString&)> Callback;

	// Find and remove callback
	{
		FScopeLock Lock(&CallbackLock);
		if (TFunction<void(const FString&)>* FoundCallback = CallbackRegistry.Find(CallbackId))
		{
			Callback = *FoundCallback;
			CallbackRegistry.Remove(CallbackId);
			UE_LOG(LogTapAndroid, Verbose, TEXT("TriggerCallback: ID=%lld found and removed"), CallbackId);
		}
		else
		{
			UE_LOG(LogTapAndroid, Warning, TEXT("TriggerCallback: ID=%lld not found"), CallbackId);
			return;
		}
	}

	// Execute callback on game thread
	AsyncTask(ENamedThreads::GameThread, [Callback, ResultJson]()
	{
		Callback(ResultJson);
	});
}

bool FTapCoreAndroid::DispatchGlobalCallback(int64 CallbackId, const FString& ResultJson)
{
	// Try TapCore's own callbacks first
	{
		FScopeLock Lock(&CallbackLock);
		if (CallbackRegistry.Find(CallbackId))
		{
			UE_LOG(LogTapAndroid, Verbose, TEXT("[Global Dispatch] Found in TapCore registry"));
			TriggerCallback(CallbackId, ResultJson);
			return true;
		}
	}

	// Try other modules' callback handlers
	for (const auto& Handler : GlobalCallbackHandlers)
	{
		if (Handler(CallbackId, ResultJson))
		{
			UE_LOG(LogTapAndroid, Verbose, TEXT("[Global Dispatch] Handled by registered module"));
			return true;
		}
	}

	UE_LOG(LogTapAndroid, Warning, TEXT("[Global Dispatch] No handler found for callback ID: %lld"), CallbackId);
	return false;
}

void FTapCoreAndroid::RegisterGlobalCallbackHandler(TFunction<bool(int64, const FString&)> Handler)
{
	GlobalCallbackHandlers.Add(Handler);
	UE_LOG(LogTapAndroid, Log, TEXT("[Global Dispatch] Registered new callback handler, total: %d"),
		GlobalCallbackHandlers.Num());
}

// =============================================================================
// Helper Methods - Static
// =============================================================================

FString FTapCoreAndroid::CreateRouteRequest(const FString& MethodName, const FString& ParamsJson)
{
	TSharedPtr<FJsonObject> RequestObject = MakeShareable(new FJsonObject);
	RequestObject->SetStringField(TEXT("method"), MethodName);

	// Android 端的 RouteRequest 要求 params 是必需字段
	// 即使为空也要添加空字符串
	RequestObject->SetStringField(TEXT("params"), ParamsJson.IsEmpty() ? TEXT("") : ParamsJson);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RequestObject.ToSharedRef(), Writer);

	return OutputString;
}

bool FTapCoreAndroid::ParseRouteResponse(const FString& ResponseJson, FString& OutData, FString& OutMessage)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutMessage = TEXT("Failed to parse response JSON");
		return false;
	}

	bool bSuccess = JsonObject->GetBoolField(TEXT("success"));

	if (bSuccess)
	{
		if (JsonObject->HasField(TEXT("data")))
		{
			OutData = JsonObject->GetStringField(TEXT("data"));
		}
		return true;
	}
	else
	{
		if (JsonObject->HasField(TEXT("message")))
		{
			OutMessage = JsonObject->GetStringField(TEXT("message"));
		}
		return false;
	}
}

void FTapCoreAndroid::ApplyScreenOrientation(int32 Orientation)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("JNI environment is null, cannot apply screen orientation"));
		return;
	}

	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (!GameActivity)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("GameActivity is null, cannot apply screen orientation"));
		return;
	}

	jclass ActivityClass = Env->GetObjectClass(GameActivity);
	if (!ActivityClass)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to get Activity class for setRequestedOrientation"));
		return;
	}

	jmethodID SetOrientationMethod = Env->GetMethodID(ActivityClass, "setRequestedOrientation", "(I)V");
	if (!SetOrientationMethod)
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Failed to find setRequestedOrientation method"));
		Env->DeleteLocalRef(ActivityClass);
		return;
	}

	// Config: 0=Portrait, 1=Landscape
	// Android ActivityInfo constants:
	//   SCREEN_ORIENTATION_PORTRAIT  = 1 (locked portrait, no sensor)
	//   SCREEN_ORIENTATION_LANDSCAPE = 0 (locked landscape, no sensor)
	jint AndroidOrientation = (Orientation == 1) ? 0 : 1;

	UE_LOG(LogTapAndroid, Log, TEXT("Applying screen orientation: %s (Android value: %d)"),
		Orientation == 1 ? TEXT("Landscape") : TEXT("Portrait"), (int32)AndroidOrientation);

	Env->CallVoidMethod(GameActivity, SetOrientationMethod, AndroidOrientation);

	if (Env->ExceptionCheck())
	{
		UE_LOG(LogTapAndroid, Error, TEXT("Exception occurred when calling setRequestedOrientation"));
		Env->ExceptionDescribe();
		Env->ExceptionClear();
	}

	Env->DeleteLocalRef(ActivityClass);
}

void FTapCoreAndroid::RequestGatekeeperValidation(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapAndroid, Log, TEXT("Gatekeeper validation handled by Android Native SDK (tap-core)"));

	// The Android tap-core SDK (4.9.5-alpha.1) handles gatekeeper automatically
	// No explicit call needed from UE side
}

#endif // PLATFORM_ANDROID

// Copyright TapTap. All Rights Reserved.

#include "Platform/TapEventAndroid.h"

#if PLATFORM_ANDROID

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Dom/JsonObject.h"
#include "Internal/Platform/TapCoreAndroid.h"
#include "TapCoreModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// TapDB Router方法名称
// 注意：这些方法名必须与 Android 端 TapCoreUERouter.kt 中的定义完全一致
namespace TapDB
{
	namespace RouterMethods
	{
		// 初始化相关
		static const FString INIT = TEXT("init");
		static const FString SET_USER_ID = TEXT("setUserId");
		static const FString CLEAR_USER = TEXT("clearUser");
		static const FString GET_DEVICE_ID = TEXT("getDeviceId");

		// 事件追踪
		static const FString LOG_EVENT = TEXT("logEvent");
		static const FString LOG_PURCHASED_EVENT = TEXT("logPurchaseEvent");
		static const FString LOG_DEVICE_LOGIN = TEXT("logDeviceLogin");

		// 设备属性
		static const FString DEVICE_INITIALIZE = TEXT("deviceInitialize");
		static const FString DEVICE_UPDATE = TEXT("deviceUpdate");
		static const FString DEVICE_ADD = TEXT("deviceAdd");

		// 用户属性
		static const FString USER_INITIALIZE = TEXT("userInitialize");
		static const FString USER_UPDATE = TEXT("userUpdate");
		static const FString USER_ADD = TEXT("userAdd");

		// 公共属性
		static const FString ADD_COMMON_PROPERTY = TEXT("addCommonProperty");
		static const FString ADD_COMMON_PROPERTIES = TEXT("addCommonProperties");
		static const FString CLEAR_COMMON_PROPERTY = TEXT("clearCommonProperty");
		static const FString CLEAR_COMMON_PROPERTIES = TEXT("clearCommonProperties");
		static const FString CLEAR_ALL_COMMON = TEXT("clearAllCommonProperties");

		// 其他
		static const FString SET_OAID = TEXT("setOAID");
		static const FString REGISTER_DYNAMIC_PROPERTIES = TEXT("registerDynamicProperties");
	}
}

namespace
{
	FString SerializeJsonObject(const TSharedRef<FJsonObject>& JsonObject)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(JsonObject, Writer);
		return Json;
	}

	bool TrySetJsonObjectField(
		const TSharedRef<FJsonObject>& Destination,
		const FString& FieldName,
		const FString& ObjectJson,
		const TCHAR* Context)
	{
		if (ObjectJson.TrimStartAndEnd().IsEmpty())
		{
			return true;
		}

		TSharedPtr<FJsonObject> ParsedObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ObjectJson);
		if (!FJsonSerializer::Deserialize(Reader, ParsedObject) || !ParsedObject.IsValid())
		{
			UE_LOG(LogTapCore, Error, TEXT("[TapEvent] %s failed: properties must be a valid JSON object"), Context);
			return false;
		}

		Destination->SetObjectField(FieldName, ParsedObject);
		return true;
	}
}

// =============================================================================
// Static: Dynamic Properties JNI Bridge
// =============================================================================

// C++ 端存储的动态属性回调，供 JNI 同步调用
static TFunction<FString()>* s_AndroidDynamicPropertiesCallback = nullptr;

/**
 * JNI native method: EngineBridgeCallbackProxy.nativeGetDynamicProperties()
 * Android SDK 每次发送事件时，通过此 JNI 方法同步获取 C++ 端的动态属性
 */
extern "C" JNIEXPORT jstring JNICALL
Java_com_taptap_sdk_kit_internal_enginebridge_EngineBridgeCallbackProxy_nativeGetDynamicProperties(
	JNIEnv* Env,
	jclass Clazz)
{
	if (s_AndroidDynamicPropertiesCallback && *s_AndroidDynamicPropertiesCallback)
	{
		FString Result = (*s_AndroidDynamicPropertiesCallback)();
		return Env->NewStringUTF(TCHAR_TO_UTF8(*Result));
	}

	return Env->NewStringUTF("{}");
}

// =============================================================================

FTapEventAndroid::FTapEventAndroid()
	: bIsInitialized(false)
	, DynamicPropertiesCallback(nullptr)
{
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Android implementation created"));
}

FTapEventAndroid::~FTapEventAndroid()
{
	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Android implementation destroyed"));
}

void FTapEventAndroid::Init(bool bEnableLog)
{
	bIsInitialized = true;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] Android Init, EnableLog=%s"), bEnableLog ? TEXT("true") : TEXT("false"));

	// TapDB的初始化由TapCore统一处理，这里只记录状态
}

void FTapEventAndroid::SetUserID(const FString& UserID)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] SetUserID: %s"), *UserID);

	const TSharedRef<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetStringField(TEXT("userId"), UserID);
	const FString ParamsJson = SerializeJsonObject(ParamsObject);
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::SET_USER_ID, ParamsJson);

	UE_LOG(LogTapCore, Verbose, TEXT("[TapEvent] SetUserID result: %s"), *Result);
}

void FTapEventAndroid::SetUserID(const FString& UserID, const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] SetUserID: %s, Properties: %s"), *UserID, *Properties);

	const TSharedRef<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetStringField(TEXT("userId"), UserID);
	if (!TrySetJsonObjectField(ParamsObject, TEXT("properties"), Properties, TEXT("SetUserID")))
	{
		return;
	}
	const FString ParamsJson = SerializeJsonObject(ParamsObject);
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::SET_USER_ID, ParamsJson);

	UE_LOG(LogTapCore, Verbose, TEXT("[TapEvent] SetUserID with properties result: %s"), *Result);
}

void FTapEventAndroid::ClearUser()
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] ClearUser"));

	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::CLEAR_USER);
	UE_LOG(LogTapCore, Verbose, TEXT("[TapEvent] ClearUser result: %s"), *Result);
}

FString FTapEventAndroid::GetDeviceId()
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTapCore, Warning, TEXT("[TapEvent] Not initialized, returning empty device ID"));
		return TEXT("");
	}

	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::GET_DEVICE_ID);

	UE_LOG(LogTapCore, Verbose, TEXT("[TapEvent] GetDeviceId result: %s"), *Result);

	// 解析返回的 JSON: {"success": true, "data": "deviceId"}
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result);
	if (FJsonSerializer::Deserialize(Reader, JsonObject) && JsonObject.IsValid())
	{
		FString DeviceId;
		if (JsonObject->TryGetStringField(TEXT("data"), DeviceId))
		{
			return DeviceId;
		}
	}

	UE_LOG(LogTapCore, Warning, TEXT("[TapEvent] Failed to parse device ID from JSON: %s"), *Result);
	return Result;
}

void FTapEventAndroid::LogEvent(const FString& Name, const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] LogEvent: %s, Properties: %s"), *Name, *Properties);

	const TSharedRef<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetStringField(TEXT("eventName"), Name);
	if (!TrySetJsonObjectField(ParamsObject, TEXT("properties"), Properties, TEXT("LogEvent")))
	{
		return;
	}
	const FString ParamsJson = SerializeJsonObject(ParamsObject);
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::LOG_EVENT, ParamsJson);

	UE_LOG(LogTapCore, Verbose, TEXT("[TapEvent] LogEvent result: %s"), *Result);
}

void FTapEventAndroid::DeviceInitialize(const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] DeviceInitialize: %s"), *Properties);

	// 直接传递 Properties JSON
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::DEVICE_INITIALIZE, Properties);
}

void FTapEventAndroid::DeviceUpdate(const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] DeviceUpdate: %s"), *Properties);

	// 直接传递 Properties JSON
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::DEVICE_UPDATE, Properties);
}

void FTapEventAndroid::DeviceAdd(const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] DeviceAdd: %s"), *Properties);

	// 直接传递 Properties JSON
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::DEVICE_ADD, Properties);
}

void FTapEventAndroid::UserInitialize(const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] UserInitialize: %s"), *Properties);

	// 直接传递 Properties JSON
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::USER_INITIALIZE, Properties);
}

void FTapEventAndroid::UserUpdate(const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] UserUpdate: %s"), *Properties);

	// 直接传递 Properties JSON
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::USER_UPDATE, Properties);
}

void FTapEventAndroid::UserAdd(const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] UserAdd: %s"), *Properties);

	// 直接传递 Properties JSON
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::USER_ADD, Properties);
}

void FTapEventAndroid::AddCommonProperty(const FString& Key, const FString& Value)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] AddCommonProperty: %s = %s"), *Key, *Value);

	const TSharedRef<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetStringField(TEXT("key"), Key);
	ParamsObject->SetStringField(TEXT("value"), Value);
	const FString ParamsJson = SerializeJsonObject(ParamsObject);
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::ADD_COMMON_PROPERTY, ParamsJson);
}

void FTapEventAndroid::AddCommon(const FString& Properties)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] AddCommon: %s"), *Properties);

	// 直接传递 Properties JSON，不需要包装
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::ADD_COMMON_PROPERTIES, Properties);
}

void FTapEventAndroid::ClearCommonProperty(const FString& Key)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] ClearCommonProperty: %s"), *Key);

	const TSharedRef<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetStringField(TEXT("key"), Key);
	const FString ParamsJson = SerializeJsonObject(ParamsObject);
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::CLEAR_COMMON_PROPERTY, ParamsJson);
}

void FTapEventAndroid::ClearCommonProperties(const TArray<FString>& Keys)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] ClearCommonProperties: %d keys"), Keys.Num());

	TArray<TSharedPtr<FJsonValue>> JsonKeys;
	JsonKeys.Reserve(Keys.Num());
	for (const FString& Key : Keys)
	{
		JsonKeys.Add(MakeShared<FJsonValueString>(Key));
	}

	const TSharedRef<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetArrayField(TEXT("keys"), JsonKeys);
	const FString ParamsJson = SerializeJsonObject(ParamsObject);
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::CLEAR_COMMON_PROPERTIES, ParamsJson);
}

void FTapEventAndroid::ClearAllCommonProperties()
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] ClearAllCommonProperties"));

	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::CLEAR_ALL_COMMON);
}

void FTapEventAndroid::LogPurchasedEvent(const FString& OrderJson)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] LogPurchasedEvent: %s"), *OrderJson);

	// 直接传递 JSON 字符串给 Android 端
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::LOG_PURCHASED_EVENT, OrderJson);

	UE_LOG(LogTapCore, Verbose, TEXT("[TapEvent] LogPurchasedEvent result: %s"), *Result);
}

void FTapEventAndroid::RegisterDynamicProperties(TFunction<FString()> Callback)
{
	if (!bIsInitialized) return;

	DynamicPropertiesCallback = Callback;

	// 将回调指针存到静态变量，供 JNI nativeGetDynamicProperties 同步访问
	s_AndroidDynamicPropertiesCallback = &DynamicPropertiesCallback;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] RegisterDynamicProperties callback registered"));

	// 通知 Android SDK 注册 TapEventDynamicProperties 实现
	// Android 端会创建一个 TapEventDynamicProperties，
	// 其 getDynamicProperties() 通过 JNI 调用 nativeGetDynamicProperties() 同步获取 C++ 回调结果
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::REGISTER_DYNAMIC_PROPERTIES);

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] RegisterDynamicProperties route result: %s"), *Result);
}

void FTapEventAndroid::SetOAID(const FString& Value)
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] SetOAID: %s"), *Value);

	const TSharedRef<FJsonObject> ParamsObject = MakeShared<FJsonObject>();
	ParamsObject->SetStringField(TEXT("oaid"), Value);
	const FString ParamsJson = SerializeJsonObject(ParamsObject);
	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::SET_OAID, ParamsJson);
}

void FTapEventAndroid::LogDeviceLoginEvent()
{
	if (!bIsInitialized) return;

	UE_LOG(LogTapCore, Log, TEXT("[TapEvent] LogDeviceLoginEvent"));

	FString Result = FTapCoreAndroid::CallModuleRoute(TapDB::RouterMethods::LOG_DEVICE_LOGIN);
}

#endif // PLATFORM_ANDROID

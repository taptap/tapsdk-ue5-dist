// Copyright TapTap. All Rights Reserved.

#include "Platform/TapComplianceAndroid.h"
#include "Internal/TapLogPrivacy.h"

#if PLATFORM_ANDROID

#include "Internal/Platform/TapCoreAndroid.h"

#include "TapCompliance.h"
#include "TapCompliancePaymentResult.h"
#include "TapComplianceModule.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Async/Async.h"
#include "Misc/ScopeExit.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

// =============================================================================
// Static Members
// =============================================================================

TFunction<void(int32, const FString&)> FTapComplianceAndroid::ComplianceEventCallback;
FCriticalSection FTapComplianceAndroid::ComplianceCallbackLock;
FTapComplianceAsyncRequests FTapComplianceAndroid::PendingRequests;

// =============================================================================
// JNI Native Method: nativeOnComplianceEvent
// Called from Java: TapComplianceUERouter.nativeOnComplianceEvent(int, String)
// 持久回调入口 - 每次合规事件都会触发，不消耗
// =============================================================================

extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_compliance_internal_ue_TapComplianceUERouter_nativeOnComplianceEvent(
	JNIEnv* Env,
	jclass  Cls,
	jint    Code,
	jstring ExtraJson)
{
	if (Env == nullptr)
	{
		return;
	}

	FString ExtraJsonStr;
	if (ExtraJson != nullptr)
	{
		const char* Chars = Env->GetStringUTFChars(ExtraJson, nullptr);
		ExtraJsonStr = FString(UTF8_TO_TCHAR(Chars));
		Env->ReleaseStringUTFChars(ExtraJson, Chars);
	}

	UE_LOG(LogTapCompliance, Log, TEXT("[JNI] nativeOnComplianceEvent: Code=%d, Extra=%s"),
		(int32)Code, *ExtraJsonStr);

	FTapComplianceAndroid::DispatchComplianceEvent((int32)Code, ExtraJsonStr);
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapComplianceAndroid::FTapComplianceAndroid()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] FTapComplianceAndroid created"));

	// 注册 TapCompliance 的 per-call 回调处理器到 TapCore 全局分发器
	// 用于 checkPaymentLimit / submitPayment 的一次性回调路由
	FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 CallbackId, const FString& ResultJson) -> bool
	{
		return PendingRequests.Complete(CallbackId, ResultJson);
	});

	UE_LOG(LogTapCompliance, Log, TEXT("[Android] Per-call callback handler registered with TapCore global dispatcher"));
}

FTapComplianceAndroid::~FTapComplianceAndroid()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] FTapComplianceAndroid destroyed"));
}

void FTapComplianceAndroid::ApplyInitOptions(const FTapComplianceOptions& Options)
{
	// TapCore forwards registered module options to TapTapSdk.init before this task runs.
}

// =============================================================================
// ITapCompliancePlatform Interface Implementation
// =============================================================================

void FTapComplianceAndroid::RegisterComplianceCallback(TFunction<void(int32, const FString&)> Callback)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] RegisterComplianceCallback called"));

	// 1. 存储 C++ 回调（nativeOnComplianceEvent 会调用此回调）
	{
		FScopeLock Lock(&ComplianceCallbackLock);
		ComplianceEventCallback = Callback;
	}

	// 2. 通知 Java 侧注册合规事件监听（同步调用）
	FString ResultJson = CallModuleRoute(TEXT("registerComplianceCallback"), TEXT(""));
	FString Data, Message;
	if (!ParseRouteResponse(ResultJson, Data, Message))
	{
		UE_LOG(LogTapCompliance, Warning, TEXT("[Android] RegisterComplianceCallback route warning: %s"), *Message);
	}

	UE_LOG(LogTapCompliance, Log, TEXT("[Android] Compliance callback registered successfully"));
}

void FTapComplianceAndroid::UnregisterComplianceCallback()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] UnregisterComplianceCallback called"));

	// 1. 清除 C++ 侧持久回调
	{
		FScopeLock Lock(&ComplianceCallbackLock);
		ComplianceEventCallback = nullptr;
	}

	// 2. 通知 Java 侧注销合规事件监听
	FString ResultJson = CallModuleRoute(TEXT("unregisterComplianceCallback"), TEXT(""));
	FString Data, Message;
	if (!ParseRouteResponse(ResultJson, Data, Message))
	{
		UE_LOG(LogTapCompliance, Warning, TEXT("[Android] UnregisterComplianceCallback route warning: %s"), *Message);
	}
}

void FTapComplianceAndroid::Startup(const FString& UserId)
{
	UE_LOG(LogTapCompliance, Log, TEXT("========================================"));
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] Startup: UserId=%s"), *UserId);

	// 构造参数 JSON
	TSharedPtr<FJsonObject> ParamsObj = MakeShareable(new FJsonObject);
	ParamsObj->SetStringField(TEXT("userId"), UserId);

	FString ParamsJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsJson);
	FJsonSerializer::Serialize(ParamsObj.ToSharedRef(), Writer);

	FString ResultJson = CallModuleRoute(TEXT("startup"), ParamsJson);

	FString Data, Message;
	if (!ParseRouteResponse(ResultJson, Data, Message))
	{
		UE_LOG(LogTapCompliance, Warning, TEXT("[Android] Startup warning: %s"), *Message);
	}

	UE_LOG(LogTapCompliance, Log, TEXT("[Android] Startup initiated, compliance events will arrive via nativeOnComplianceEvent"));
	UE_LOG(LogTapCompliance, Log, TEXT("========================================"));
}

void FTapComplianceAndroid::Exit()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] Exit called"));

	FString ResultJson = CallModuleRoute(TEXT("exit"), TEXT(""));

	FString Data, Message;
	if (ParseRouteResponse(ResultJson, Data, Message))
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[Android] Exit success"));
	}
	else
	{
		UE_LOG(LogTapCompliance, Warning, TEXT("[Android] Exit warning: %s"), *Message);
	}
}

void FTapComplianceAndroid::CheckPaymentLimit(
	int64 Amount,
	TFunction<void(int32, const FString&, const FString&)> Callback,
	TFunction<void(const FString&)> OnError)
{
	UE_LOG(LogTapCompliance, Log, TEXT("========================================"));
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] CheckPaymentLimit: Amount=%lld"), Amount);

	TSharedPtr<FJsonObject> ParamsObj = MakeShareable(new FJsonObject);
	ParamsObj->SetNumberField(TEXT("amount"), static_cast<double>(Amount));

	FString ParamsJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsJson);
	FJsonSerializer::Serialize(ParamsObj.ToSharedRef(), Writer);

	CallModuleRouteAsync(TEXT("checkPaymentLimit"), ParamsJson, [Callback, OnError](const FString& ResultJson)
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[Android] CheckPaymentLimit callback: %s"), *ResultJson);

		FString Data, Message;
		if (ParseRouteResponse(ResultJson, Data, Message))
		{
			TapCompliancePaymentResult::Dispatch(Data, Callback, OnError);
		}
		else
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[Android] CheckPaymentLimit failed: %s"), *Message);
			if (OnError) OnError(Message);
		}

		UE_LOG(LogTapCompliance, Log, TEXT("========================================"));
	});

	UE_LOG(LogTapCompliance, Log, TEXT("[Android] CheckPaymentLimit request sent, waiting for callback..."));
	UE_LOG(LogTapCompliance, Log, TEXT("========================================"));
}

void FTapComplianceAndroid::SubmitPayment(
	int64 Amount,
	TFunction<void(bool, const FString&)> Callback)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] SubmitPayment: Amount=%lld"), Amount);

	TSharedPtr<FJsonObject> ParamsObj = MakeShareable(new FJsonObject);
	ParamsObj->SetNumberField(TEXT("amount"), static_cast<double>(Amount));

	FString ParamsJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsJson);
	FJsonSerializer::Serialize(ParamsObj.ToSharedRef(), Writer);

	CallModuleRouteAsync(TEXT("submitPayment"), ParamsJson, [Callback](const FString& ResultJson)
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[Android] SubmitPayment callback: %s"), *ResultJson);

		FString Data, Message;
		if (ParseRouteResponse(ResultJson, Data, Message))
		{
			UE_LOG(LogTapCompliance, Log, TEXT("[Android] SubmitPayment success"));
			Callback(true, TEXT(""));
		}
		else
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[Android] SubmitPayment failed: %s"), *Message);
			Callback(false, Message);
		}
	});
}

int32 FTapComplianceAndroid::GetAgeRange()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] GetAgeRange called"));

	FString ResultJson = CallModuleRoute(TEXT("getAgeRange"), TEXT(""));

	FString Data, Message;
	if (ParseRouteResponse(ResultJson, Data, Message))
	{
		TSharedPtr<FJsonObject> DataObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
		if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
		{
			int32 AgeRange = (int32)DataObj->GetNumberField(TEXT("ageRange"));
			UE_LOG(LogTapCompliance, Log, TEXT("[Android] GetAgeRange: %d"), AgeRange);
			return AgeRange;
		}
	}

	UE_LOG(LogTapCompliance, Warning, TEXT("[Android] GetAgeRange failed: %s"), *Message);
	return -1;
}

int32 FTapComplianceAndroid::GetUserRemainTime()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] GetUserRemainTime called"));

	FString ResultJson = CallModuleRoute(TEXT("getUserRemainTime"), TEXT(""));

	FString Data, Message;
	if (ParseRouteResponse(ResultJson, Data, Message))
	{
		TSharedPtr<FJsonObject> DataObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
		if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
		{
			int32 RemainTime = (int32)DataObj->GetNumberField(TEXT("remainTime"));
			UE_LOG(LogTapCompliance, Log, TEXT("[Android] GetUserRemainTime: %d seconds"), RemainTime);
			return FMath::Max(0, RemainTime);
		}
	}

	UE_LOG(LogTapCompliance, Warning, TEXT("[Android] GetUserRemainTime failed: %s"), *Message);
	return 0;
}

FString FTapComplianceAndroid::GetCurrentAccessToken()
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Android] GetCurrentAccessToken called"));

	FString ResultJson = CallModuleRoute(TEXT("getCurrentAccessToken"), TEXT(""));
	FString Data, Message;
	if (ParseRouteResponse(ResultJson, Data, Message))
	{
		TSharedPtr<FJsonObject> DataObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
		if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
		{
			return DataObj->HasField(TEXT("token"))
				? DataObj->GetStringField(TEXT("token"))
				: FString();
		}
	}

	UE_LOG(LogTapCompliance, Warning, TEXT("[Android] GetCurrentAccessToken failed: %s"), *Message);
	return FString();
}

// =============================================================================
// DispatchComplianceEvent - Called from JNI native method (persistent callback)
// =============================================================================

void FTapComplianceAndroid::DispatchComplianceEvent(int32 Code, const FString& ExtraJson)
{
	UE_LOG(LogTapCompliance, Log, TEXT("========================================"));
	UE_LOG(LogTapCompliance, Log, TEXT("[Dispatch] ComplianceEvent: Code=%d, Extra=%s"), Code, *ExtraJson);

	TFunction<void(int32, const FString&)> Callback;
	{
		FScopeLock Lock(&ComplianceCallbackLock);
		Callback = ComplianceEventCallback;
	}

	if (!Callback)
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Dispatch] No compliance callback registered, event dropped: Code=%d"), Code);
		return;
	}

	// 在 Game Thread 上执行回调（不从注册表移除，持久回调）
	AsyncTask(ENamedThreads::GameThread, [Callback, Code, ExtraJson]()
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[Dispatch] Executing compliance callback on GameThread: Code=%d"), Code);
		Callback(Code, ExtraJson);
	});

	UE_LOG(LogTapCompliance, Log, TEXT("========================================"));
}

// =============================================================================
// Router Methods - Bridge to Android SDK via routeToModule
// =============================================================================

FString FTapComplianceAndroid::CallModuleRoute(const FString& MethodName, const FString& ParamsJson)
{
	UE_LOG(LogTapCompliance, Verbose, TEXT("[Router] CallModuleRoute: %s"), *MethodName);

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[Router] JNI environment is null"));
		return TEXT("{\"success\":false,\"message\":\"JNI environment is null\"}");
	}

	FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);

	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (GameActivity == nullptr)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[Router] GameActivity is null"));
		return TEXT("{\"success\":false,\"message\":\"GameActivity is null\"}");
	}

	jclass ActivityClass = Env->GetObjectClass(GameActivity);
	jmethodID RouteMethod = Env->GetMethodID(
		ActivityClass,
		"routeToTapCompliance",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
	);

	if (RouteMethod == nullptr)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[Router] routeToTapCompliance method not found"));
		Env->DeleteLocalRef(ActivityClass);
		return TEXT("{\"success\":false,\"message\":\"routeToTapCompliance method not found\"}");
	}

	jstring jRequestJson = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));

	jstring jResult = (jstring)Env->CallObjectMethod(
		GameActivity, RouteMethod, jRequestJson, nullptr
	);

	if (Env->ExceptionCheck())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[Router] JNI exception during routeToTapCompliance"));
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		Env->DeleteLocalRef(jRequestJson);
		Env->DeleteLocalRef(ActivityClass);
		return TEXT("{\"success\":false,\"message\":\"JNI exception occurred\"}");
	}

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

	Env->DeleteLocalRef(jRequestJson);
	Env->DeleteLocalRef(ActivityClass);

	UE_LOG(LogTapCompliance, Verbose, TEXT("[Router] CallModuleRoute result: %s"), *TapLogPrivacy::Body(Result, MethodName == TEXT("getCurrentAccessToken")));
	return Result;
}

void FTapComplianceAndroid::CallModuleRouteAsync(
	const FString& MethodName,
	const FString& ParamsJson,
	TFunction<void(const FString&)> Callback)
{
	const int64 CallbackId = PendingRequests.Add([Callback = MoveTemp(Callback)](const FString& Response)
	{
		AsyncTask(ENamedThreads::GameThread, [Callback, Response]()
		{
			if (Callback) Callback(Response);
		});
	});
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	auto Fail = [Env, CallbackId](const TCHAR* Message)
	{
		// JNI lookups and allocations can leave an exception pending. Clear it before cleanup.
		if (Env && Env->ExceptionCheck()) Env->ExceptionClear();
		PendingRequests.Fail(CallbackId, Message);
	};
	if (!Env)
	{
		Fail(TEXT("JNI environment is null"));
		return;
	}
	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (!GameActivity)
	{
		Fail(TEXT("GameActivity is null"));
		return;
	}

	jclass ActivityClass = Env->GetObjectClass(GameActivity);
	jobject CallbackObj = nullptr;
	jstring Request = nullptr;
	jstring Result = nullptr;
	ON_SCOPE_EXIT
	{
		if (Result) Env->DeleteLocalRef(Result);
		if (Request) Env->DeleteLocalRef(Request);
		if (CallbackObj) Env->DeleteLocalRef(CallbackObj);
		if (ActivityClass) Env->DeleteLocalRef(ActivityClass);
	};
	if (!ActivityClass || Env->ExceptionCheck())
	{
		Fail(TEXT("Failed to find GameActivity class"));
		return;
	}
	jmethodID RouteMethod = Env->GetMethodID(ActivityClass, "routeToTapCompliance",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;");
	if (!RouteMethod || Env->ExceptionCheck())
	{
		Fail(TEXT("routeToTapCompliance method not found"));
		return;
	}
	CallbackObj = CreateEngineBridgeCallback(Env, CallbackId);
	if (!CallbackObj || Env->ExceptionCheck())
	{
		Fail(TEXT("Failed to create callback object"));
		return;
	}
	const FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);
	Request = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));
	if (!Request || Env->ExceptionCheck())
	{
		Fail(TEXT("Failed to create route request"));
		return;
	}
	// Keep the local proxy reference alive even if Java completes synchronously and
	// Complete() releases its global reference before CallObjectMethod returns.
	Result = static_cast<jstring>(Env->CallObjectMethod(GameActivity, RouteMethod, Request, CallbackObj));
	if (Env->ExceptionCheck())
	{
		Fail(TEXT("JNI exception during async routeToTapCompliance"));
		return;
	}
	if (!Result)
	{
		Fail(TEXT("TapCompliance route returned no response"));
		return;
	}
	const char* Chars = Env->GetStringUTFChars(Result, nullptr);
	if (!Chars || Env->ExceptionCheck())
	{
		Fail(TEXT("Failed to read TapCompliance route response"));
		if (Chars) Env->ReleaseStringUTFChars(Result, Chars);
		return;
	}
	const FString Response(UTF8_TO_TCHAR(Chars));
	Env->ReleaseStringUTFChars(Result, Chars);
	PendingRequests.HandleImmediateResponse(CallbackId, Response);
}

jobject FTapComplianceAndroid::CreateEngineBridgeCallback(JNIEnv* Env, int64 CallbackId)
{
	// Use the application class loader without the engine's fatal missing-class check.
	jobject Loader = FAndroidApplication::GetClassLoader();
	if (!Loader) return nullptr;
	jclass LoaderClass = Env->GetObjectClass(Loader);
	jstring ClassName = nullptr;
	jclass ProxyClass = nullptr;
	ON_SCOPE_EXIT
	{
		if (ProxyClass) Env->DeleteLocalRef(ProxyClass);
		if (ClassName) Env->DeleteLocalRef(ClassName);
		if (LoaderClass) Env->DeleteLocalRef(LoaderClass);
	};
	if (!LoaderClass || Env->ExceptionCheck()) return nullptr;
	jmethodID LoadClass = Env->GetMethodID(LoaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	if (!LoadClass || Env->ExceptionCheck()) return nullptr;
	ClassName = Env->NewStringUTF("com.taptap.sdk.kit.internal.enginebridge.EngineBridgeCallbackProxy");
	if (!ClassName || Env->ExceptionCheck()) return nullptr;
	ProxyClass = static_cast<jclass>(Env->CallObjectMethod(Loader, LoadClass, ClassName));
	if (!ProxyClass || Env->ExceptionCheck()) return nullptr;
	jmethodID Constructor = Env->GetMethodID(ProxyClass, "<init>", "(J)V");
	if (!Constructor || Env->ExceptionCheck()) return nullptr;
	jobject Proxy = Env->NewObject(ProxyClass, Constructor, static_cast<jlong>(CallbackId));
	if (!Proxy || Env->ExceptionCheck())
	{
		if (Proxy) Env->DeleteLocalRef(Proxy);
		return nullptr;
	}
	jobject GlobalProxy = Env->NewGlobalRef(Proxy);
	if (GlobalProxy)
	{
		PendingRequests.AttachCleanup(CallbackId, [GlobalProxy]()
		{
			// Completion may arrive on a Java thread; never reuse the creation thread's JNIEnv.
			JNIEnv* CleanupEnv = FAndroidApplication::GetJavaEnv(false);
			if (ensure(CleanupEnv)) CleanupEnv->DeleteGlobalRef(GlobalProxy);
		});
	}
	if (!GlobalProxy || Env->ExceptionCheck())
	{
		Env->DeleteLocalRef(Proxy);
		return nullptr;
	}
	return Proxy;
}

// =============================================================================
// Helper Methods
// =============================================================================

FString FTapComplianceAndroid::CreateRouteRequest(const FString& MethodName, const FString& ParamsJson)
{
	TSharedPtr<FJsonObject> RequestObject = MakeShareable(new FJsonObject);
	RequestObject->SetStringField(TEXT("method"), MethodName);

	if (!ParamsJson.IsEmpty())
	{
		RequestObject->SetStringField(TEXT("params"), ParamsJson);
	}

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(RequestObject.ToSharedRef(), Writer);

	return OutputString;
}

bool FTapComplianceAndroid::ParseRouteResponse(
	const FString& ResponseJson,
	FString& OutData,
	FString& OutMessage)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutMessage = TEXT("Failed to parse response JSON");
		return false;
	}

	if (!JsonObject->HasField(TEXT("success")))
	{
		OutMessage = TEXT("Response JSON missing 'success' field");
		return false;
	}

	bool bSuccess = JsonObject->GetBoolField(TEXT("success"));

	if (bSuccess)
	{
		if (JsonObject->HasField(TEXT("data")))
		{
			// data 可能是 JSON 对象或字符串
			TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
			if (DataObject.IsValid())
			{
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutData);
				FJsonSerializer::Serialize(DataObject.ToSharedRef(), Writer);
			}
			else
			{
				OutData = JsonObject->GetStringField(TEXT("data"));
			}
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

#endif // PLATFORM_ANDROID

// Copyright TapTap. All Rights Reserved.
#include "Mobile/Android/TapLicenseAndroid.h"

#if PLATFORM_ANDROID

#include "Internal/Platform/TapCoreAndroid.h"
#include "Internal/TapAndroidCallbackId.h"
#include "TapLicenseModule.h"
#include "Async/Async.h"
#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Dom/JsonObject.h"
#include "TapJsonCompat.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// =============================================================================
// Static Members
// =============================================================================

FTapLicenseAndroid::FLicenseSuccessFn FTapLicenseAndroid::CbSuccess;
FTapLicenseAndroid::FLicenseFailedFn  FTapLicenseAndroid::CbFailed;
FTapLicenseAndroid::FDLCQueryFn       FTapLicenseAndroid::CbDLCQuery;
FTapLicenseAndroid::FDLCOrderFn       FTapLicenseAndroid::CbDLCOrder;
FCriticalSection                      FTapLicenseAndroid::CallbackLock;

TMap<int64, TFunction<void(const FString&)>> FTapLicenseAndroid::PerCallRegistry;
FCriticalSection FTapLicenseAndroid::PerCallLock;

// =============================================================================
// JNI Persistent Callback Entry Points
// =============================================================================

extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_license_internal_ue_TapLicenseUERouter_nativeOnLicenseEvent(
	JNIEnv* Env, jclass, jboolean Owned)
{
	FTapLicenseAndroid::DispatchLicenseEvent(Owned == JNI_TRUE);
}

extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_license_internal_ue_TapLicenseUERouter_nativeOnDLCQueryResult(
	JNIEnv* Env, jclass, jstring JResultJson)
{
	if (!Env || !JResultJson) { return; }
	const char* Chars = Env->GetStringUTFChars(JResultJson, nullptr);
	FString ResultJson(UTF8_TO_TCHAR(Chars));
	Env->ReleaseStringUTFChars(JResultJson, Chars);
	FTapLicenseAndroid::DispatchDLCQueryResult(ResultJson);
}

extern "C" JNIEXPORT void JNICALL
Java_com_taptap_sdk_license_internal_ue_TapLicenseUERouter_nativeOnDLCOrderEvent(
	JNIEnv* Env, jclass, jstring Sku, jint Status)
{
	if (!Env) { return; }
	FString SkuStr;
	if (Sku)
	{
		const char* Chars = Env->GetStringUTFChars(Sku, nullptr);
		SkuStr = FString(UTF8_TO_TCHAR(Chars));
		Env->ReleaseStringUTFChars(Sku, Chars);
	}
	FTapLicenseAndroid::DispatchDLCOrderEvent(SkuStr, (int32)Status);
}

// =============================================================================
// Constructor
// =============================================================================

FTapLicenseAndroid::FTapLicenseAndroid()
{
	// 注册 per-call 回调处理器到 TapCore 全局分发器（供 QueryDLC 使用）
	FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 CallbackId, const FString& ResultJson) -> bool
	{
		FScopeLock Lock(&FTapLicenseAndroid::PerCallLock);
		if (FTapLicenseAndroid::PerCallRegistry.Find(CallbackId))
		{
			FTapLicenseAndroid::TriggerPerCallCallback(CallbackId, ResultJson);
			return true;
		}
		return false;
	});
	UE_LOG(LogTapLicense, Log, TEXT("[Android] TapLicense per-call callback handler registered"));
}

// =============================================================================
// ITapLicensePlatform Interface
// =============================================================================

void FTapLicenseAndroid::SetupCallbacks(
	FLicenseSuccessFn OnSuccess, FLicenseFailedFn OnFailed,
	FDLCQueryFn OnDLCQuery, FDLCOrderFn OnDLCOrder)
{
	FScopeLock Lock(&CallbackLock);
	CbSuccess  = MoveTemp(OnSuccess);
	CbFailed   = MoveTemp(OnFailed);
	CbDLCQuery = MoveTemp(OnDLCQuery);
	CbDLCOrder = MoveTemp(OnDLCOrder);
}

void FTapLicenseAndroid::Initialize()
{
	// TapLicenseInitTask 在 TapCore::Initialize() 内执行，此时 TapSdkChecker
	// 仍可能认为 SDK 尚未完成初始化。若此时调用 registerCallback，Android
	// API 会静默拒绝注册，但 UEBridge 会缓存 callback 并误报 already registered。
	// 将注册延后到首次 License/DLC API 调用时，此时 TapCore 已完成初始化。
	UE_LOG(LogTapLicense, Log, TEXT("[Android] TapLicense initialized; native callback registration deferred until first API call"));
}

bool FTapLicenseAndroid::EnsureNativeCallbacksRegistered()
{
	FString Result = CallModuleRoute(TEXT("registerCallback"), TEXT(""));
	FString Data, Message;
	if (ParseRouteResponse(Result, Data, Message))
	{
		UE_LOG(LogTapLicense, Log, TEXT("[Android] TapLicense callback registered successfully"));
		return true;
	}

	UE_LOG(LogTapLicense, Warning, TEXT("[Android] TapLicense registerCallback failed: %s (response=%s)"),
		*Message, *Result);
	return false;
}

void FTapLicenseAndroid::Shutdown()
{
	FScopeLock Lock(&CallbackLock);
	CbSuccess = nullptr; CbFailed = nullptr; CbDLCQuery = nullptr; CbDLCOrder = nullptr;
}

void FTapLicenseAndroid::CheckLicense(bool bForce)
{
	if (!EnsureNativeCallbacksRegistered())
	{
		DispatchLicenseEvent(false);
		return;
	}

	const FString Params = FString::Printf(TEXT("{\"force\":%s}"), bForce ? TEXT("true") : TEXT("false"));
	// 同步发起，结果通过 nativeOnLicenseEvent JNI 持久回调返回
	const FString Result = CallModuleRoute(TEXT("checkLicense"), Params);
	FString Data, Message;
	if (!ParseRouteResponse(Result, Data, Message))
	{
		UE_LOG(LogTapLicense, Warning, TEXT("[Android] checkLicense route failed: %s (response=%s)"),
			*Message, *Result);
		DispatchLicenseEvent(false);
	}
}

void FTapLicenseAndroid::QueryDLC(const TArray<FString>& DLCList)
{
	if (!EnsureNativeCallbacksRegistered())
	{
		DispatchDLCQueryResult(TEXT("{\"code\":2,\"results\":{}}"));
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& S : DLCList) { Arr.Add(MakeShared<FJsonValueString>(S)); }
	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetArrayField(TEXT("skuIds"), Arr);
	FString Params;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params);
	FJsonSerializer::Serialize(Obj, W);
	// 发起查询；结果通过 nativeOnDLCQueryResult JNI 持久回调返回
	const FString Result = CallModuleRoute(TEXT("queryDLC"), Params);
	FString Data, Message;
	if (!ParseRouteResponse(Result, Data, Message))
	{
		UE_LOG(LogTapLicense, Warning, TEXT("[Android] queryDLC route failed: %s (response=%s)"),
			*Message, *Result);
		DispatchDLCQueryResult(TEXT("{\"code\":2,\"results\":{}}"));
	}
}

void FTapLicenseAndroid::DispatchDLCQueryResult(const FString& ResultJson)
{
	// 解析 {"code":N,"results":{"sku1":true,...}}
	ETapLicenseQueryCode Code = ETapLicenseQueryCode::OK;
	FTapLicenseDLCResultMap Map;

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResultJson);
	if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
	{
		if (Root->HasField(TEXT("code")))
			Code = static_cast<ETapLicenseQueryCode>((int32)Root->GetNumberField(TEXT("code")));
		const TSharedPtr<FJsonObject>* ResultsObj;
		if (Root->TryGetObjectField(TEXT("results"), ResultsObj))
		{
			for (const auto& Pair : (*ResultsObj)->Values)
				Map.Add(TapJson::KeyString(Pair.Key), Pair.Value->AsBool());
		}
	}
	else
	{
		Code = ETapLicenseQueryCode::Error;
		UE_LOG(LogTapLicense, Warning, TEXT("[Android] DispatchDLCQueryResult parse failed: %s"), *ResultJson);
	}

	FScopeLock Lock(&CallbackLock);
	FDLCQueryFn Query = CbDLCQuery;
	AsyncTask(ENamedThreads::GameThread, [Query, Code, Map]()
	{
		if (Query) Query(Code, Map);
	});
}

void FTapLicenseAndroid::PurchaseDLC(const FString& DLCID)
{
	if (!EnsureNativeCallbacksRegistered())
	{
		DispatchDLCOrderEvent(DLCID, TapLicenseDLCStatus::ToNativeValue(ETapDLCPurchasedCode::ReturnError));
		return;
	}

	TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("skuId"), DLCID);
	FString Params;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Params);
	FJsonSerializer::Serialize(Obj, W);

	// 同步发起，订单状态通过 nativeOnDLCOrderEvent JNI 持久回调返回
	const FString Result = CallModuleRoute(TEXT("purchaseDLC"), Params);
	FString Data, Message;
	if (!ParseRouteResponse(Result, Data, Message))
	{
		UE_LOG(LogTapLicense, Warning, TEXT("[Android] purchaseDLC route failed: %s (response=%s)"),
			*Message, *Result);
		DispatchDLCOrderEvent(DLCID, TapLicenseDLCStatus::ToNativeValue(ETapDLCPurchasedCode::ReturnError));
	}
}

// =============================================================================
// Dispatch (called from JNI entry points, may be on non-game thread)
// =============================================================================

void FTapLicenseAndroid::DispatchLicenseEvent(bool bOwned)
{
	FScopeLock Lock(&CallbackLock);
	FLicenseSuccessFn Success = CbSuccess;
	FLicenseFailedFn  Failed  = CbFailed;
	AsyncTask(ENamedThreads::GameThread, [Success, Failed, bOwned]()
	{
		if (bOwned) { if (Success) Success(); }
		else        { if (Failed)  Failed();  }
	});
}

void FTapLicenseAndroid::DispatchDLCOrderEvent(const FString& Sku, int32 Status)
{
	FScopeLock Lock(&CallbackLock);
	FDLCOrderFn Order = CbDLCOrder;
	const ETapDLCPurchasedCode Code = TapLicenseDLCStatus::FromNativeValue(Status);
	AsyncTask(ENamedThreads::GameThread, [Order, Sku, Code]()
	{
		if (Order) Order(Sku, Code);
	});
}

void FTapLicenseAndroid::TriggerPerCallCallback(int64 CallbackId, const FString& ResultJson)
{
	TFunction<void(const FString&)> Cb;
	{
		FScopeLock Lock(&PerCallLock);
		if (TFunction<void(const FString&)>* Found = PerCallRegistry.Find(CallbackId))
		{
			Cb = *Found;
			PerCallRegistry.Remove(CallbackId);
		}
	}
	if (Cb)
	{
		AsyncTask(ENamedThreads::GameThread, [Cb, ResultJson]() { Cb(ResultJson); });
	}
}

// =============================================================================
// Router — calls routeToTapLicense on GameActivity (mirrors TapCompliance pattern)
// =============================================================================

FString FTapLicenseAndroid::CallModuleRoute(const FString& MethodName, const FString& ParamsJson)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env) return TEXT("{\"success\":false,\"message\":\"JNI null\"}");

	FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);

	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (!GameActivity) return TEXT("{\"success\":false,\"message\":\"GameActivity null\"}");

	jclass  ActivityClass = Env->GetObjectClass(GameActivity);
	jmethodID RouteMethod = Env->GetMethodID(
		ActivityClass,
		"routeToTapLicense",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
	);

	if (!RouteMethod || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
		UE_LOG(LogTapLicense, Error, TEXT("[Android] routeToTapLicense method not found"));
		Env->DeleteLocalRef(ActivityClass);
		return TEXT("{\"success\":false,\"message\":\"routeToTapLicense not found\"}");
	}

	jstring jReq = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));
	jstring jRes = (jstring)Env->CallObjectMethod(GameActivity, RouteMethod, jReq, nullptr);

	if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }

	FString Result;
	if (jRes)
	{
		const char* C = Env->GetStringUTFChars(jRes, nullptr);
		Result = FString(UTF8_TO_TCHAR(C));
		Env->ReleaseStringUTFChars(jRes, C);
		Env->DeleteLocalRef(jRes);
	}
	else { Result = TEXT("{\"success\":false,\"message\":\"null result\"}"); }

	Env->DeleteLocalRef(jReq);
	Env->DeleteLocalRef(ActivityClass);
	return Result;
}

void FTapLicenseAndroid::CallModuleRouteAsync(
	const FString& MethodName, const FString& ParamsJson,
	TFunction<void(const FString&)> Callback)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (!Env)
	{
		AsyncTask(ENamedThreads::GameThread, [Callback]()
		{ Callback(TEXT("{\"success\":false,\"message\":\"JNI null\"}")); });
		return;
	}

	FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);

	jobject CallbackObj = CreateEngineBridgeCallback(Env, Callback);
	if (!CallbackObj)
	{
		AsyncTask(ENamedThreads::GameThread, [Callback]()
		{ Callback(TEXT("{\"success\":false,\"message\":\"callback create failed\"}")); });
		return;
	}

	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (!GameActivity)
	{
		DestroyEngineBridgeCallback(Env, CallbackObj);
		AsyncTask(ENamedThreads::GameThread, [Callback]()
		{ Callback(TEXT("{\"success\":false,\"message\":\"GameActivity null\"}")); });
		return;
	}

	jclass    ActivityClass = Env->GetObjectClass(GameActivity);
	jmethodID RouteMethod   = Env->GetMethodID(
		ActivityClass,
		"routeToTapLicense",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
	);

	if (!RouteMethod || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
		UE_LOG(LogTapLicense, Error, TEXT("[Android Async] routeToTapLicense method not found"));
		Env->DeleteLocalRef(ActivityClass);
		DestroyEngineBridgeCallback(Env, CallbackObj);
		AsyncTask(ENamedThreads::GameThread, [Callback]()
		{ Callback(TEXT("{\"success\":false,\"message\":\"routeToTapLicense not found\"}")); });
		return;
	}

	jstring jReq = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));
	jstring jRes = (jstring)Env->CallObjectMethod(GameActivity, RouteMethod, jReq, CallbackObj);

	if (Env->ExceptionCheck()) { Env->ExceptionDescribe(); Env->ExceptionClear(); }
	if (jRes) { Env->DeleteLocalRef(jRes); }

	Env->DeleteLocalRef(jReq);
	Env->DeleteLocalRef(ActivityClass);
	// CallbackObj（global ref）在 TriggerPerCallCallback 时释放
}

// =============================================================================
// EngineBridgeCallbackProxy Management
// =============================================================================

jobject FTapLicenseAndroid::CreateEngineBridgeCallback(JNIEnv* Env, TFunction<void(const FString&)> Callback)
{
	int64 CallbackId;
	{
		FScopeLock Lock(&PerCallLock);
		CallbackId = FTapAndroidCallbackId::Allocate();
		PerCallRegistry.Add(CallbackId, Callback);
	}

	jclass ProxyClass = FAndroidApplication::FindJavaClass(
		"com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy"
	);
	if (!ProxyClass)
	{
		UE_LOG(LogTapLicense, Error, TEXT("[Callback] EngineBridgeCallbackProxy not found"));
		FScopeLock Lock(&PerCallLock); PerCallRegistry.Remove(CallbackId);
		return nullptr;
	}

	jmethodID Ctor = Env->GetMethodID(ProxyClass, "<init>", "(J)V");
	if (!Ctor)
	{
		Env->DeleteLocalRef(ProxyClass);
		FScopeLock Lock(&PerCallLock); PerCallRegistry.Remove(CallbackId);
		return nullptr;
	}

	jobject Obj = Env->NewObject(ProxyClass, Ctor, (jlong)CallbackId);
	Env->DeleteLocalRef(ProxyClass);
	if (!Obj)
	{
		FScopeLock Lock(&PerCallLock); PerCallRegistry.Remove(CallbackId);
		return nullptr;
	}

	jobject Global = Env->NewGlobalRef(Obj);
	Env->DeleteLocalRef(Obj);
	return Global;
}

void FTapLicenseAndroid::DestroyEngineBridgeCallback(JNIEnv* Env, jobject CallbackObj)
{
	if (Env && CallbackObj) Env->DeleteGlobalRef(CallbackObj);
}

// =============================================================================
// Helpers
// =============================================================================

FString FTapLicenseAndroid::CreateRouteRequest(const FString& MethodName, const FString& ParamsJson)
{
	TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
	Obj->SetStringField(TEXT("method"), MethodName);
	if (!ParamsJson.IsEmpty()) Obj->SetStringField(TEXT("params"), ParamsJson);
	FString Out;
	TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(Obj.ToSharedRef(), W);
	return Out;
}

bool FTapLicenseAndroid::ParseRouteResponse(const FString& Json, FString& OutData, FString& OutMsg)
{
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(R, Obj) || !Obj.IsValid()) { OutMsg = TEXT("parse failed"); return false; }
	if (!Obj->HasField(TEXT("success"))) { OutMsg = TEXT("missing success"); return false; }
	if (Obj->GetBoolField(TEXT("success")))
	{
		if (Obj->HasField(TEXT("data")))
		{
			TSharedPtr<FJsonObject> DataObj = Obj->GetObjectField(TEXT("data"));
			if (DataObj.IsValid())
			{
				TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&OutData);
				FJsonSerializer::Serialize(DataObj.ToSharedRef(), W);
			}
			else { OutData = Obj->GetStringField(TEXT("data")); }
		}
		return true;
	}
	if (Obj->HasField(TEXT("message"))) OutMsg = Obj->GetStringField(TEXT("message"));
	return false;
}

#endif // PLATFORM_ANDROID

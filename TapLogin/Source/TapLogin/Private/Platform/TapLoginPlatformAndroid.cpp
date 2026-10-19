// Copyright TapTap. All Rights Reserved.

#include "TapLoginPlatformAndroid.h"
#include "Internal/TapLogPrivacy.h"
#include "TapLoginModule.h"

#if PLATFORM_ANDROID

#include "Internal/Platform/TapCoreAndroid.h"
#include "Internal/TapAndroidCallbackId.h"

#include "Android/AndroidJNI.h"
#include "Android/AndroidApplication.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Async/Async.h"

// =============================================================================
// Static Member Initialization
// =============================================================================

TMap<int64, FTapLoginPlatformAndroid::FCallbackEntry> FTapLoginPlatformAndroid::CallbackRegistry;
FCriticalSection FTapLoginPlatformAndroid::CallbackLock;

// =============================================================================
// JNI Native Callback Entry Point
// =============================================================================
// Note: TapLogin shares the same EngineBridgeCallbackProxy with TapCore
// The callback will be routed to the correct module based on callback ID

FTapLoginPlatformAndroid::FTapLoginPlatformAndroid()
	: bInitialized(false)
{
	UE_LOG(LogTapLogin, Log, TEXT("[Android] FTapLoginPlatformAndroid constructor called"));

	// Register TapLogin's callback handler with TapCore's global dispatcher
	// This allows TapLogin callbacks to be routed correctly from the shared JNI entry point
	FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 CallbackId, const FString& ResultJson) -> bool
	{
		return FTapLoginPlatformAndroid::TriggerCallback(CallbackId, ResultJson);
	});

	UE_LOG(LogTapLogin, Log, TEXT("[Android] TapLogin callback handler registered with global dispatcher"));
}

FTapLoginPlatformAndroid::~FTapLoginPlatformAndroid()
{
	UE_LOG(LogTapLogin, Log, TEXT("[Android] FTapLoginPlatformAndroid destructor called"));
}

void FTapLoginPlatformAndroid::Initialize()
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android] TapLogin Platform Initialization"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));

	if (bInitialized)
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[Android] TapLogin is already initialized"));
		return;
	}

	UE_LOG(LogTapLogin, Log, TEXT("[Android] Initializing TapLogin..."));

	bInitialized = true;
	UE_LOG(LogTapLogin, Log, TEXT("[Android] TapLogin initialized successfully"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android] Platform: %s"), *GetPlatformName());
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
}

bool FTapLoginPlatformAndroid::IsInitialized() const
{
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android] IsInitialized called, returning: %s"), bInitialized ? TEXT("true") : TEXT("false"));
	return bInitialized;
}

void FTapLoginPlatformAndroid::LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback)
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android] LoginWithScopes called"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android] Scopes count: %d"), Scopes.Num());

	if (Scopes.Num() == 0)
	{
		if (Callback.GetInterface())
		{
			Callback->OnLoginFailed(-1, TEXT("Scope cannot be empty"));
		}
		return;
	}

	// 打印所有权限范围
	for (int32 i = 0; i < Scopes.Num(); i++)
	{
		FString ScopeStr = ScopeToString(Scopes[i]);
		UE_LOG(LogTapLogin, Log, TEXT("[Android]   Scope[%d]: %s"), i, *ScopeStr);
	}

	// 检查回调有效性
	if (!Callback.GetInterface())
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Android] Callback is null!"));
		return;
	}
	UE_LOG(LogTapLogin, Log, TEXT("[Android] Callback is valid"));

	// 将 Scopes 转换为 JSON 数组
	TArray<FString> ScopeStrings = ScopesToStringArray(Scopes);
	TSharedPtr<FJsonObject> ParamsObj = MakeShareable(new FJsonObject);
	TArray<TSharedPtr<FJsonValue>> ScopeJsonArray;
	for (const FString& ScopeStr : ScopeStrings)
	{
		ScopeJsonArray.Add(MakeShareable(new FJsonValueString(ScopeStr)));
	}
	ParamsObj->SetArrayField(TEXT("scopes"), ScopeJsonArray);

	FString ParamsJson;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsJson);
	FJsonSerializer::Serialize(ParamsObj.ToSharedRef(), Writer);

	UE_LOG(LogTapLogin, Log, TEXT("[Android] Calling Router with params: %s"), *ParamsJson);

	// 调用 Android SDK 登录方法（异步）
	CallModuleRouteAsync(TEXT("loginWithScopes"), ParamsJson, [Callback](const FString& ResultJson)
	{
		UE_LOG(LogTapLogin, Log, TEXT("[Android] Login callback received: %s"), *TapLogPrivacy::Body(ResultJson));

		TSharedPtr<FJsonObject> ResponseObject;
		const TSharedRef<TJsonReader<>> ResponseReader = TJsonReaderFactory<>::Create(ResultJson);
		const bool bResponseParsed = FJsonSerializer::Deserialize(ResponseReader, ResponseObject)
			&& ResponseObject.IsValid();
		const bool bCancelled = bResponseParsed
			&& ResponseObject->HasField(TEXT("cancelled"))
			&& ResponseObject->GetBoolField(TEXT("cancelled"));
		if (bCancelled)
		{
			if (Callback.GetInterface())
			{
				Callback->OnLoginCancelled();
			}
			return;
		}

		// 解析响应
		FString Data, Message;
		if (ParseRouteResponse(ResultJson, Data, Message))
		{
			// 成功 - 解析账号信息
			TSharedPtr<FJsonObject> DataObj;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
			if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
			{
				FTapAccount Account;
				// Android AccessToken only exposes MAC credentials, matching Unity.

				Account.OpenID = DataObj->GetStringField(TEXT("openid"));
				Account.UnionID = DataObj->HasField(TEXT("unionid")) ? DataObj->GetStringField(TEXT("unionid")) : TEXT("");
				Account.Name = DataObj->HasField(TEXT("name")) ? DataObj->GetStringField(TEXT("name")) : TEXT("");
				Account.Avatar = DataObj->HasField(TEXT("avatar")) ? DataObj->GetStringField(TEXT("avatar")) : TEXT("");
				Account.Email = DataObj->HasField(TEXT("email")) ? DataObj->GetStringField(TEXT("email")) : TEXT("");

				const TSharedPtr<FJsonObject>* TokenDetail = nullptr;
				if (DataObj->TryGetObjectField(TEXT("accessTokenDetail"), TokenDetail) && TokenDetail && TokenDetail->IsValid())
				{
					(*TokenDetail)->TryGetStringField(TEXT("kid"), Account.AccessToken.Kid);
					(*TokenDetail)->TryGetStringField(TEXT("macKey"), Account.AccessToken.MacKey);
					(*TokenDetail)->TryGetStringField(TEXT("macAlgorithm"), Account.AccessToken.MacAlgorithm);
					(*TokenDetail)->TryGetStringField(TEXT("tokenType"), Account.AccessToken.TokenType);
					FString ScopeString;
					if ((*TokenDetail)->TryGetStringField(TEXT("scopes"), ScopeString)) ScopeString.ParseIntoArray(Account.AccessToken.Scopes, TEXT(","), true);
				}

				UE_LOG(LogTapLogin, Log, TEXT("[Android] Login success:"));
				UE_LOG(LogTapLogin, Log, TEXT("  Kid: %s"), TEXT("[REDACTED]"));
				UE_LOG(LogTapLogin, Log, TEXT("  OpenID: %s"), *Account.OpenID);
				UE_LOG(LogTapLogin, Log, TEXT("  UnionID: %s"), *Account.UnionID);

				if (Callback.GetInterface())
				{
					Callback->OnLoginSuccess(Account);
				}
			}
			else
			{
				UE_LOG(LogTapLogin, Error, TEXT("[Android] Failed to parse login result data"));
				if (Callback.GetInterface())
				{
					Callback->OnLoginFailed(-1, TEXT("Failed to parse login result"));
				}
			}
		}
		else
		{
			// 失败
			UE_LOG(LogTapLogin, Error, TEXT("[Android] Login failed: %s"), *Message);

			if (Callback.GetInterface())
			{
				Callback->OnLoginFailed(-1, Message);
			}
		}

		UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	});

	UE_LOG(LogTapLogin, Log, TEXT("[Android] Login request sent, waiting for callback..."));
}

void FTapLoginPlatformAndroid::Logout()
{
	UE_LOG(LogTapLogin, Log, TEXT("[Android] Logout called"));

	// 调用 Android SDK 登出方法（同步）
	FString ResultJson = CallModuleRoute(TEXT("logout"), TEXT(""));

	FString Data, Message;
	if (ParseRouteResponse(ResultJson, Data, Message))
	{
		UE_LOG(LogTapLogin, Log, TEXT("[Android] Logout successful"));
	}
	else
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[Android] Logout failed: %s"), *Message);
	}
}

TOptional<FTapAccount> FTapLoginPlatformAndroid::GetCurrentAccount() const
{
	UE_LOG(LogTapLogin, Log, TEXT("[Android] GetCurrentAccount called"));

	// 调用 Android SDK 获取当前账号（同步）
	FString ResultJson = CallModuleRoute(TEXT("getCurrentAccount"), TEXT(""));

	FString Data, Message;
	if (ParseRouteResponse(ResultJson, Data, Message))
	{
		// 解析账号信息
		TSharedPtr<FJsonObject> DataObj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Data);
		if (FJsonSerializer::Deserialize(Reader, DataObj) && DataObj.IsValid())
		{
			FTapAccount Account;
			Account.OpenID = DataObj->GetStringField(TEXT("openid"));
			Account.UnionID = DataObj->HasField(TEXT("unionid")) ? DataObj->GetStringField(TEXT("unionid")) : TEXT("");
			Account.Name = DataObj->HasField(TEXT("name")) ? DataObj->GetStringField(TEXT("name")) : TEXT("");
			Account.Avatar = DataObj->HasField(TEXT("avatar")) ? DataObj->GetStringField(TEXT("avatar")) : TEXT("");
			Account.Email = DataObj->HasField(TEXT("email")) ? DataObj->GetStringField(TEXT("email")) : TEXT("");

			const TSharedPtr<FJsonObject>* AccessTokenDetail = nullptr;
			if (DataObj->TryGetObjectField(TEXT("accessTokenDetail"), AccessTokenDetail)
				&& AccessTokenDetail
				&& AccessTokenDetail->IsValid())
			{
				(*AccessTokenDetail)->TryGetStringField(TEXT("kid"), Account.AccessToken.Kid);
				(*AccessTokenDetail)->TryGetStringField(TEXT("macKey"), Account.AccessToken.MacKey);
				(*AccessTokenDetail)->TryGetStringField(TEXT("macAlgorithm"), Account.AccessToken.MacAlgorithm);
				(*AccessTokenDetail)->TryGetStringField(TEXT("tokenType"), Account.AccessToken.TokenType);

				FString ScopeString;
				if ((*AccessTokenDetail)->TryGetStringField(TEXT("scopes"), ScopeString))
				{
					ScopeString.ParseIntoArray(Account.AccessToken.Scopes, TEXT(","), true);
				}
			}

			UE_LOG(
				LogTapLogin,
				Log,
				TEXT("[Android] Current account found: OpenID=%s TokenKid=%s MacKeyPresent=%s"),
				*Account.OpenID,
				TEXT("[REDACTED]"),
				Account.AccessToken.MacKey.IsEmpty() ? TEXT("false") : TEXT("true"));

			return TOptional<FTapAccount>(Account);
		}
	}

	UE_LOG(LogTapLogin, Log, TEXT("[Android] No account found or failed to get: %s"), *Message);

	return TOptional<FTapAccount>();
}

FString FTapLoginPlatformAndroid::ScopeToString(ETapLoginScope Scope) const
{
	FString Result;
	switch (Scope)
	{
	case ETapLoginScope::BasicInfo:
		Result = TEXT("basic_info");
		break;
	case ETapLoginScope::PublicProfile:
		Result = TEXT("public_profile");
		break;
	case ETapLoginScope::Email:
		Result = TEXT("email");
		break;
	case ETapLoginScope::UserFriends:
		Result = TEXT("user_friends");
		break;
	case ETapLoginScope::Compliance:
		Result = TEXT("compliance");
		break;
	case ETapLoginScope::ComplianceBasic:
		Result = TEXT("compliance_basic");
		break;
	default:
		Result = TEXT("");
		UE_LOG(LogTapLogin, Warning, TEXT("[Android] Unknown scope type: %d"), (int32)Scope);
		break;
	}

	UE_LOG(LogTapLogin, Verbose, TEXT("[Android] Converted scope to string: %s"), *Result);
	return Result;
}

TArray<FString> FTapLoginPlatformAndroid::ScopesToStringArray(const TArray<ETapLoginScope>& Scopes) const
{
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android] Converting %d scopes to string array"), Scopes.Num());

	TArray<FString> Result;
	for (int32 i = 0; i < Scopes.Num(); i++)
	{
		FString ScopeStr = ScopeToString(Scopes[i]);
		Result.Add(ScopeStr);
		UE_LOG(LogTapLogin, Verbose, TEXT("[Android]   Converted scope[%d]: %s"), i, *ScopeStr);
	}

	UE_LOG(LogTapLogin, Verbose, TEXT("[Android] Scope conversion completed, result count: %d"), Result.Num());
	return Result;
}

// =============================================================================
// Router Methods - Bridge to Android SDK
// =============================================================================

FString FTapLoginPlatformAndroid::CallModuleRoute(const FString& MethodName, const FString& ParamsJson)
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router] CallModuleRoute START"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router] Method: %s"), *MethodName);
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router] Params: %s"), ParamsJson.IsEmpty() ? TEXT("<empty>") : *ParamsJson);

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router] JNI environment is null"));
		return TEXT("{\"success\":false,\"message\":\"JNI environment is null\"}");
	}

	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] JNI environment OK"));

	// Create Router request JSON
	FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router] Request JSON: %s"), *RequestJson);

	// Get GameActivity
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] Getting GameActivity..."));
	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (GameActivity == nullptr)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router] GameActivity is null"));
		return TEXT("{\"success\":false,\"message\":\"GameActivity is null\"}");
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] GameActivity OK"));

	// Get Activity class and routeToTapLogin method
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] Getting routeToTapLogin method..."));
	jclass ActivityClass = Env->GetObjectClass(GameActivity);
	jmethodID RouteMethod = Env->GetMethodID(
		ActivityClass,
		"routeToTapLogin",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
	);

	if (RouteMethod == nullptr)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router] routeToTapLogin method not found"));
		Env->DeleteLocalRef(ActivityClass);
		return TEXT("{\"success\":false,\"message\":\"routeToTapLogin method not found\"}");
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] routeToTapLogin method OK"));

	// Create Java string
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] Creating JNI string..."));
	jstring jRequestJson = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] JNI string created"));

	// Call routeToTapLogin (sync: callback = null)
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router] Calling Java routeToTapLogin(..., null)"));

	jstring jResult = (jstring)Env->CallObjectMethod(
		GameActivity, RouteMethod, jRequestJson, nullptr
	);

	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] Java call returned"));

	// Convert result
	FString Result;
	if (jResult != nullptr)
	{
		const char* ResultChars = Env->GetStringUTFChars(jResult, nullptr);
		Result = FString(UTF8_TO_TCHAR(ResultChars));
		Env->ReleaseStringUTFChars(jResult, ResultChars);
		Env->DeleteLocalRef(jResult);
		UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] Result converted from Java"));
	}
	else
	{
		Result = TEXT("{\"success\":false,\"message\":\"No result from Java\"}");
		UE_LOG(LogTapLogin, Warning, TEXT("[Android Router] Java returned null"));
	}

	// Cleanup
	Env->DeleteLocalRef(jRequestJson);
	Env->DeleteLocalRef(ActivityClass);
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router] JNI cleanup done"));

	UE_LOG(LogTapLogin, Log, TEXT("[Android Router] Result: %s"), *TapLogPrivacy::Body(Result));
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router] CallModuleRoute END"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));

	return Result;
}

void FTapLoginPlatformAndroid::CallModuleRouteAsync(
	const FString& MethodName,
	const FString& ParamsJson,
	TFunction<void(const FString&)> Callback)
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] CallModuleRouteAsync START"));
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] Method: %s"), *MethodName);
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] Params: %s"), ParamsJson.IsEmpty() ? TEXT("<empty>") : *ParamsJson);

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router Async] JNI environment is null"));
		Callback(TEXT("{\"success\":false,\"message\":\"JNI environment is null\"}"));
		return;
	}

	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] JNI environment OK"));

	// Create Router request JSON
	FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] Request JSON: %s"), *RequestJson);

	// Create callback object and register its ownership before Java can invoke it.
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] Creating callback object..."));
	int64 CallbackId = 0;
	jobject CallbackObj = CreateEngineBridgeCallback(Env, Callback, CallbackId);
	if (CallbackObj == nullptr)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router Async] Failed to create callback object"));
		Callback(TEXT("{\"success\":false,\"message\":\"Failed to create callback object\"}"));
		return;
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] Callback object created"));

	// Get GameActivity
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] Getting GameActivity..."));
	jobject GameActivity = FAndroidApplication::GetGameActivityThis();
	if (GameActivity == nullptr)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router Async] GameActivity is null"));
		FailPendingCallback(Env, CallbackId, TEXT("{\"success\":false,\"message\":\"GameActivity is null\"}"));
		return;
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] GameActivity OK"));

	// Get Activity class and routeToTapLogin method
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] Getting routeToTapLogin method..."));
	jclass ActivityClass = Env->GetObjectClass(GameActivity);
	if (ActivityClass == nullptr || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router Async] Failed to get GameActivity class"));
		FailPendingCallback(Env, CallbackId, TEXT("{\"success\":false,\"message\":\"Failed to get GameActivity class\"}"));
		return;
	}
	jmethodID RouteMethod = Env->GetMethodID(
		ActivityClass,
		"routeToTapLogin",
		"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;"
	);

	if (RouteMethod == nullptr || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router Async] routeToTapLogin method not found"));
		Env->DeleteLocalRef(ActivityClass);
		FailPendingCallback(Env, CallbackId, TEXT("{\"success\":false,\"message\":\"routeToTapLogin method not found\"}"));
		return;
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] routeToTapLogin method OK"));

	// Create Java string
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] Creating JNI string..."));
	jstring jRequestJson = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));
	if (jRequestJson == nullptr || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		Env->DeleteLocalRef(ActivityClass);
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router Async] Failed to create request string"));
		FailPendingCallback(Env, CallbackId, TEXT("{\"success\":false,\"message\":\"Failed to create JNI request string\"}"));
		return;
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] JNI string created"));

	// Call routeToTapLogin (async: pass callback)
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] Calling Java routeToTapLogin(..., callback)"));

	jstring jResult = (jstring)Env->CallObjectMethod(
		GameActivity, RouteMethod, jRequestJson, CallbackObj
	);

	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] Java call returned"));

	if (Env->ExceptionCheck())
	{
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		if (jResult != nullptr) Env->DeleteLocalRef(jResult);
		Env->DeleteLocalRef(jRequestJson);
		Env->DeleteLocalRef(ActivityClass);
		UE_LOG(LogTapLogin, Error, TEXT("[Android Router Async] JNI exception during routeToTapLogin"));
		FailPendingCallback(Env, CallbackId, TEXT("{\"success\":false,\"message\":\"JNI exception during routeToTapLogin\"}"));
		return;
	}

	FString ImmediateResult;
	if (jResult != nullptr)
	{
		const char* ResultChars = Env->GetStringUTFChars(jResult, nullptr);
		if (ResultChars != nullptr && !Env->ExceptionCheck())
		{
			ImmediateResult = FString(UTF8_TO_TCHAR(ResultChars));
			Env->ReleaseStringUTFChars(jResult, ResultChars);
		}
		else if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		Env->DeleteLocalRef(jResult);
	}
	if (ImmediateResult.IsEmpty())
	{
		ImmediateResult = TEXT("{\"success\":false,\"message\":\"routeToTapLogin returned no result\"}");
	}
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] Immediate result: %s"), *ImmediateResult);

	// Cleanup
	Env->DeleteLocalRef(jRequestJson);
	Env->DeleteLocalRef(ActivityClass);
	UE_LOG(LogTapLogin, Verbose, TEXT("[Android Router Async] JNI cleanup done"));

	TSharedPtr<FJsonObject> ImmediateObject;
	const TSharedRef<TJsonReader<>> ImmediateReader = TJsonReaderFactory<>::Create(ImmediateResult);
	bool bImmediateSuccess = false;
	bool bAsyncAccepted = false;
	const bool bAccepted = FJsonSerializer::Deserialize(ImmediateReader, ImmediateObject)
		&& ImmediateObject.IsValid()
		&& ImmediateObject->TryGetBoolField(TEXT("success"), bImmediateSuccess)
		&& bImmediateSuccess
		&& ImmediateObject->TryGetBoolField(TEXT("async"), bAsyncAccepted)
		&& bAsyncAccepted;
	if (!bAccepted)
	{
		FString ImmediateMessage;
		if (ImmediateObject.IsValid()) ImmediateObject->TryGetStringField(TEXT("message"), ImmediateMessage);
		if (ImmediateMessage.IsEmpty()) ImmediateMessage = TEXT("Java did not accept the asynchronous request");
		UE_LOG(LogTapLogin, Warning, TEXT("[Android Router Async] Java rejected async request: %s"), *ImmediateMessage);
		// Atomically taking the entry prevents a duplicate callback or double release if
		// Java completed synchronously before CallObjectMethod returned.
		FailPendingCallback(Env, CallbackId, ImmediateResult);
		return;
	}

	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] Call initiated, waiting for callback..."));
	UE_LOG(LogTapLogin, Log, TEXT("[Android Router Async] CallModuleRouteAsync END"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
}

// =============================================================================
// JNI Callback Management
// =============================================================================

jobject FTapLoginPlatformAndroid::CreateEngineBridgeCallback(
	JNIEnv* Env,
	TFunction<void(const FString&)> Callback,
	int64& OutCallbackId)
{
	UE_LOG(LogTapLogin, Log, TEXT("[Callback] CreateEngineBridgeCallback START"));

	// Generate unique callback ID
	{
		FScopeLock Lock(&CallbackLock);
		OutCallbackId = FTapAndroidCallbackId::Allocate();
		FCallbackEntry Entry;
		Entry.Callback = MoveTemp(Callback);
		CallbackRegistry.Add(OutCallbackId, MoveTemp(Entry));
	}

	UE_LOG(LogTapLogin, Log, TEXT("[Callback] Created callback ID: %lld"), OutCallbackId);

	// Find EngineBridgeCallbackProxy class
	UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] Finding EngineBridgeCallbackProxy class..."));
	jclass ProxyClass = FAndroidApplication::FindJavaClass(
		"com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy"
	);

	if (ProxyClass == nullptr || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		UE_LOG(LogTapLogin, Error, TEXT("[Callback] EngineBridgeCallbackProxy class not found"));
		FScopeLock Lock(&CallbackLock);
		CallbackRegistry.Remove(OutCallbackId);
		return nullptr;
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] EngineBridgeCallbackProxy class found"));

	// Get constructor
	UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] Getting constructor..."));
	jmethodID Constructor = Env->GetMethodID(ProxyClass, "<init>", "(J)V");
	if (Constructor == nullptr || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		UE_LOG(LogTapLogin, Error, TEXT("[Callback] EngineBridgeCallbackProxy constructor not found"));
		Env->DeleteLocalRef(ProxyClass);
		FScopeLock Lock(&CallbackLock);
		CallbackRegistry.Remove(OutCallbackId);
		return nullptr;
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] Constructor found"));

	// Create proxy object
	UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] Creating proxy object with ID: %lld"), OutCallbackId);
	jobject ProxyObj = Env->NewObject(ProxyClass, Constructor, static_cast<jlong>(OutCallbackId));
	Env->DeleteLocalRef(ProxyClass);

	if (ProxyObj == nullptr || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		if (ProxyObj != nullptr) Env->DeleteLocalRef(ProxyObj);
		UE_LOG(LogTapLogin, Error, TEXT("[Callback] Failed to create EngineBridgeCallbackProxy object"));
		FScopeLock Lock(&CallbackLock);
		CallbackRegistry.Remove(OutCallbackId);
		return nullptr;
	}
	UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] Proxy object created"));

	// Create global reference (kept alive until callback is triggered)
	UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] Creating global reference..."));
	jobject GlobalProxyObj = Env->NewGlobalRef(ProxyObj);
	Env->DeleteLocalRef(ProxyObj);
	if (GlobalProxyObj == nullptr || Env->ExceptionCheck())
	{
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
		}
		FScopeLock Lock(&CallbackLock);
		CallbackRegistry.Remove(OutCallbackId);
		UE_LOG(LogTapLogin, Error, TEXT("[Callback] Failed to create callback global reference"));
		return nullptr;
	}
	bool bStoredGlobalRef = false;
	{
		FScopeLock Lock(&CallbackLock);
		if (FCallbackEntry* Entry = CallbackRegistry.Find(OutCallbackId))
		{
			Entry->ProxyGlobal = GlobalProxyObj;
			bStoredGlobalRef = true;
		}
	}
	if (!bStoredGlobalRef)
	{
		Env->DeleteGlobalRef(GlobalProxyObj);
		UE_LOG(LogTapLogin, Warning, TEXT("[Callback] Callback registry was cleared during proxy creation"));
		return nullptr;
	}

	UE_LOG(LogTapLogin, Log, TEXT("[Callback] CreateEngineBridgeCallback END - ID: %lld"), OutCallbackId);

	return GlobalProxyObj;
}

bool FTapLoginPlatformAndroid::TakeCallbackEntry(int64 CallbackId, FCallbackEntry& OutEntry)
{
	FScopeLock Lock(&CallbackLock);
	if (FCallbackEntry* Entry = CallbackRegistry.Find(CallbackId))
	{
		OutEntry = MoveTemp(*Entry);
		CallbackRegistry.Remove(CallbackId);
		return true;
	}
	return false;
}

void FTapLoginPlatformAndroid::ReleaseCallbackEntry(JNIEnv* Env, FCallbackEntry& Entry)
{
	if (Entry.ProxyGlobal != nullptr)
	{
		if (Env != nullptr)
		{
			Env->DeleteGlobalRef(Entry.ProxyGlobal);
			UE_LOG(LogTapLogin, Verbose, TEXT("[Callback] Released callback global reference"));
		}
		else
		{
			UE_LOG(LogTapLogin, Warning, TEXT("[Callback] JNI environment unavailable while releasing global reference"));
		}
		Entry.ProxyGlobal = nullptr;
	}
}

void FTapLoginPlatformAndroid::FailPendingCallback(JNIEnv* Env, int64 CallbackId, const FString& ResultJson)
{
	FCallbackEntry Entry;
	if (!TakeCallbackEntry(CallbackId, Entry))
	{
		// The Java callback may have completed synchronously and already consumed it.
		return;
	}

	ReleaseCallbackEntry(Env, Entry);
	if (Entry.Callback)
	{
		Entry.Callback(ResultJson);
	}
}

void FTapLoginPlatformAndroid::ShutdownCallbacks()
{
	TArray<FCallbackEntry> Entries;
	{
		FScopeLock Lock(&CallbackLock);
		Entries.Reserve(CallbackRegistry.Num());
		for (TPair<int64, FCallbackEntry>& Pair : CallbackRegistry)
		{
			Entries.Add(MoveTemp(Pair.Value));
		}
		CallbackRegistry.Empty();
	}

	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	for (FCallbackEntry& Entry : Entries)
	{
		ReleaseCallbackEntry(Env, Entry);
	}

	if (!Entries.IsEmpty())
	{
		UE_LOG(LogTapLogin, Log, TEXT("[Callback] Released %d pending callbacks during shutdown"), Entries.Num());
	}
}

bool FTapLoginPlatformAndroid::TriggerCallback(int64 CallbackId, const FString& ResultJson)
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("[Callback] TriggerCallback START"));
	UE_LOG(LogTapLogin, Log, TEXT("[Callback] Callback ID: %lld"), CallbackId);
	UE_LOG(LogTapLogin, Log, TEXT("[Callback] Result JSON: %s"), *TapLogPrivacy::Body(ResultJson));

	FCallbackEntry Entry;
	if (!TakeCallbackEntry(CallbackId, Entry))
	{
		return false;
	}
	ReleaseCallbackEntry(FAndroidApplication::GetJavaEnv(), Entry);
	UE_LOG(LogTapLogin, Log, TEXT("[Callback] Found, removed and released callback ID: %lld"), CallbackId);

	// Execute callback on game thread
	UE_LOG(LogTapLogin, Log, TEXT("[Callback] Scheduling callback execution on game thread..."));

	TFunction<void(const FString&)> Callback = MoveTemp(Entry.Callback);
	AsyncTask(ENamedThreads::GameThread, [Callback = MoveTemp(Callback), ResultJson, CallbackId]() mutable
	{
		UE_LOG(LogTapLogin, Log, TEXT("[Callback] Executing callback ID: %lld on game thread"), CallbackId);
		if (Callback) Callback(ResultJson);
		UE_LOG(LogTapLogin, Log, TEXT("[Callback] Callback ID: %lld executed"), CallbackId);
	});

	UE_LOG(LogTapLogin, Log, TEXT("[Callback] TriggerCallback END"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	return true;
}

// =============================================================================
// Helper Methods
// =============================================================================

FString FTapLoginPlatformAndroid::CreateRouteRequest(const FString& MethodName, const FString& ParamsJson)
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

bool FTapLoginPlatformAndroid::ParseRouteResponse(const FString& ResponseJson, FString& OutData, FString& OutMessage)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ResponseJson);

	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		OutMessage = TEXT("Failed to parse response JSON");
		return false;
	}

	// 检查 success 字段是否存在
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
			// data 字段可能是字符串或对象
			// 先尝试作为对象获取，如果失败则作为字符串获取
			TSharedPtr<FJsonObject> DataObject = JsonObject->GetObjectField(TEXT("data"));
			if (DataObject.IsValid())
			{
				// data 是 JSON 对象，序列化为字符串
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutData);
				FJsonSerializer::Serialize(DataObject.ToSharedRef(), Writer);
			}
			else
			{
				// data 是字符串，直接获取
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

// Copyright TapTap. All Rights Reserved.

#include "Mobile/Android/TapBattleAndroidBridge.h"

#if PLATFORM_ANDROID

#include "TapBattleModule.h"
#include "Internal/Platform/TapCoreAndroid.h"
#include "Internal/TapAndroidCallbackId.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Dom/JsonObject.h"
#include "HAL/CriticalSection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include <atomic>

namespace
{
	struct FBattleCallbackEntry
	{
		TapBattleAndroidBridge::FResultCallback OneShot;
		TFunction<void(const FString&)> Notification;
		jobject ProxyGlobal = nullptr;
		bool bPersistent = false;
	};

	FCriticalSection GCallbackLock;
	TMap<int64, FBattleCallbackEntry> GCallbackRegistry;
	int64 GNotificationCallbackId = 0;
	bool GInitialized = false;
	std::atomic<jmethodID> GRouteMethodId{ nullptr };

	FString SerializeObject(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	FString CreateRouteRequest(const FString& MethodName, const FString& ParamsJson)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("method"), MethodName);
		if (!ParamsJson.IsEmpty())
		{
			Root->SetStringField(TEXT("params"), ParamsJson);
		}
		return SerializeObject(Root);
	}

	bool ParseSyncResponse(const FString& Json, FString& OutMessage)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutMessage = TEXT("Failed to parse route response JSON");
			return false;
		}

		bool bSuccess = false;
		if (!Root->TryGetBoolField(TEXT("success"), bSuccess))
		{
			OutMessage = TEXT("Response missing success field");
			return false;
		}
		if (!bSuccess)
		{
			Root->TryGetStringField(TEXT("message"), OutMessage);
			if (OutMessage.IsEmpty())
			{
				OutMessage = TEXT("Route returned success=false");
			}
			return false;
		}
		return true;
	}

	FString BuildFailureEnvelope(const FString& Message)
	{
		TSharedRef<FJsonObject> Inner = MakeShared<FJsonObject>();
		Inner->SetNumberField(TEXT("errorCode"), -1);
		Inner->SetStringField(TEXT("errorMessage"), Message);
		const FString InnerStr = SerializeObject(Inner);

		TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetNumberField(TEXT("code"), -1);
		Envelope->SetStringField(TEXT("message"), TEXT("fail"));
		Envelope->SetStringField(TEXT("content"), InnerStr);
		return SerializeObject(Envelope);
	}

	jobject CreateProxyForId(JNIEnv* Env, int64 CallbackId)
	{
		jclass ProxyClass = FAndroidApplication::FindJavaClass(
			"com/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallbackProxy");
		if (ProxyClass == nullptr)
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
			}
			UE_LOG(LogTapBattle, Error, TEXT("[TapBattle][Android] EngineBridgeCallbackProxy class not found"));
			return nullptr;
		}

		jmethodID Constructor = Env->GetMethodID(ProxyClass, "<init>", "(J)V");
		if (Constructor == nullptr || Env->ExceptionCheck())
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
			}
			Env->DeleteLocalRef(ProxyClass);
			UE_LOG(LogTapBattle, Error, TEXT("[TapBattle][Android] EngineBridgeCallbackProxy constructor not found"));
			return nullptr;
		}

		jobject ProxyLocal = Env->NewObject(ProxyClass, Constructor, static_cast<jlong>(CallbackId));
		Env->DeleteLocalRef(ProxyClass);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			if (ProxyLocal != nullptr)
			{
				Env->DeleteLocalRef(ProxyLocal);
			}
			UE_LOG(LogTapBattle, Error, TEXT("[TapBattle][Android] Failed to create EngineBridgeCallbackProxy"));
			return nullptr;
		}
		if (ProxyLocal == nullptr)
		{
			return nullptr;
		}

		jobject ProxyGlobal = Env->NewGlobalRef(ProxyLocal);
		Env->DeleteLocalRef(ProxyLocal);
		return ProxyGlobal;
	}

	FString InvokeRoute(JNIEnv* Env, const FString& MethodName, const FString& ParamsJson, jobject CallbackProxy)
	{
		const FString RequestJson = CreateRouteRequest(MethodName, ParamsJson);

		jobject GameActivity = FAndroidApplication::GetGameActivityThis();
		if (GameActivity == nullptr)
		{
			return TEXT("{\"success\":false,\"message\":\"GameActivity is null\"}");
		}

		jclass ActivityClass = Env->GetObjectClass(GameActivity);
		if (ActivityClass == nullptr || Env->ExceptionCheck())
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
			}
			if (ActivityClass != nullptr)
			{
				Env->DeleteLocalRef(ActivityClass);
			}
			return TEXT("{\"success\":false,\"message\":\"GetObjectClass(GameActivity) failed\"}");
		}

		jmethodID RouteMethod = GRouteMethodId.load(std::memory_order_acquire);
		if (RouteMethod == nullptr)
		{
			RouteMethod = Env->GetMethodID(
				ActivityClass,
				"routeToTapBattle",
				"(Ljava/lang/String;Lcom/taptap/sdk/kit/internal/enginebridge/EngineBridgeCallback;)Ljava/lang/String;");

			if (RouteMethod == nullptr || Env->ExceptionCheck())
			{
				if (Env->ExceptionCheck())
				{
					Env->ExceptionDescribe();
					Env->ExceptionClear();
				}
				Env->DeleteLocalRef(ActivityClass);
				return TEXT("{\"success\":false,\"message\":\"routeToTapBattle method not found\"}");
			}
			GRouteMethodId.store(RouteMethod, std::memory_order_release);
		}

		jstring JRequestJson = Env->NewStringUTF(TCHAR_TO_UTF8(*RequestJson));
		if (JRequestJson == nullptr || Env->ExceptionCheck())
		{
			if (Env->ExceptionCheck())
			{
				Env->ExceptionDescribe();
				Env->ExceptionClear();
			}
			if (JRequestJson != nullptr)
			{
				Env->DeleteLocalRef(JRequestJson);
			}
			Env->DeleteLocalRef(ActivityClass);
			return TEXT("{\"success\":false,\"message\":\"NewStringUTF failed\"}");
		}

		jstring JResult = static_cast<jstring>(
			Env->CallObjectMethod(GameActivity, RouteMethod, JRequestJson, CallbackProxy));

		FString Result;
		if (Env->ExceptionCheck())
		{
			Env->ExceptionDescribe();
			Env->ExceptionClear();
			if (JResult != nullptr)
			{
				Env->DeleteLocalRef(JResult);
			}
			Result = TEXT("{\"success\":false,\"message\":\"JNI exception during routeToTapBattle\"}");
		}
		else if (JResult != nullptr)
		{
			const char* Chars = Env->GetStringUTFChars(JResult, nullptr);
			Result = FString(UTF8_TO_TCHAR(Chars));
			Env->ReleaseStringUTFChars(JResult, Chars);
			Env->DeleteLocalRef(JResult);
		}
		else
		{
			Result = TEXT("{\"success\":false,\"message\":\"routeToTapBattle returned null\"}");
		}

		Env->DeleteLocalRef(JRequestJson);
		Env->DeleteLocalRef(ActivityClass);
		return Result;
	}

	bool DispatchCallback(int64 CallbackId, const FString& ResultJson)
	{
		bool bPersistent = false;
		TapBattleAndroidBridge::FResultCallback OneShot;
		TFunction<void(const FString&)> Notification;
		jobject ProxyToRelease = nullptr;
		{
			FScopeLock Lock(&GCallbackLock);
			FBattleCallbackEntry* Entry = GCallbackRegistry.Find(CallbackId);
			if (Entry == nullptr)
			{
				return false;
			}

			bPersistent = Entry->bPersistent;
			if (bPersistent)
			{
				Notification = Entry->Notification;
			}
			else
			{
				OneShot = MoveTemp(Entry->OneShot);
				ProxyToRelease = Entry->ProxyGlobal;
				GCallbackRegistry.Remove(CallbackId);
			}
		}

		if (bPersistent)
		{
			if (Notification)
			{
				Notification(ResultJson);
			}
			return true;
		}

		if (OneShot)
		{
			OneShot(true, ResultJson);
		}
		if (ProxyToRelease != nullptr)
		{
			if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
			{
				Env->DeleteGlobalRef(ProxyToRelease);
			}
		}
		return true;
	}


	FString BuildParamsWithNumber(const FString& FieldName, double Value)
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetNumberField(FieldName, Value);
		return SerializeObject(Params);
	}

	FString BuildParamsWithString(const FString& FieldName, const FString& Value)
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetStringField(FieldName, Value);
		return SerializeObject(Params);
	}

	FString BuildParamsWithBool(const FString& NumberFieldName, int32 NumberValue, const FString& BoolFieldName, bool bBoolValue)
	{
		TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
		Params->SetNumberField(NumberFieldName, NumberValue);
		Params->SetBoolField(BoolFieldName, bBoolValue);
		return SerializeObject(Params);
	}

	bool ParseSyncDataString(const FString& Json, FString& OutData)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		return Root->TryGetStringField(TEXT("data"), OutData);
	}

	bool ParseSyncDataInt64(const FString& Json, int64& OutData)
	{
		TSharedPtr<FJsonObject> Root;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		FString StringValue;
		if (Root->TryGetStringField(TEXT("data"), StringValue))
		{
			OutData = FCString::Atoi64(*StringValue);
			return true;
		}
		double Number = 0.0;
		if (!Root->TryGetNumberField(TEXT("data"), Number))
		{
			return false;
		}
		OutData = static_cast<int64>(Number);
		return true;
	}

	bool ParseSyncDataInt32(const FString& Json, int32& OutData)
	{
		int64 Value = 0;
		if (!ParseSyncDataInt64(Json, Value))
		{
			return false;
		}
		OutData = static_cast<int32>(Value);
		return true;
	}


	FString DispatchSyncRoute(const FString& MethodName, const FString& ParamsJson)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (Env == nullptr)
		{
			return TEXT("{\"success\":false,\"message\":\"JNIEnv is null\"}");
		}
		return InvokeRoute(Env, MethodName, ParamsJson, nullptr);
	}
	void DispatchOneShotRoute(const FString& MethodName, const FString& ParamsJson, TapBattleAndroidBridge::FResultCallback OnResult)
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		if (Env == nullptr)
		{
			if (OnResult)
			{
				OnResult(false, TEXT(""));
			}
			return;
		}

		int64 CallbackId = 0;
		{
			FScopeLock Lock(&GCallbackLock);
			CallbackId = FTapAndroidCallbackId::Allocate();
			FBattleCallbackEntry Entry;
			Entry.OneShot = OnResult;
			GCallbackRegistry.Add(CallbackId, MoveTemp(Entry));
		}

		jobject ProxyGlobal = CreateProxyForId(Env, CallbackId);
		if (ProxyGlobal == nullptr)
		{
			{
				FScopeLock Lock(&GCallbackLock);
				GCallbackRegistry.Remove(CallbackId);
			}
			if (OnResult)
			{
				OnResult(false, TEXT(""));
			}
			return;
		}

		{
			FScopeLock Lock(&GCallbackLock);
			if (FBattleCallbackEntry* Entry = GCallbackRegistry.Find(CallbackId))
			{
				Entry->ProxyGlobal = ProxyGlobal;
			}
		}

		const FString SyncResponse = InvokeRoute(Env, MethodName, ParamsJson, ProxyGlobal);

		bool bStillRegistered = false;
		{
			FScopeLock Lock(&GCallbackLock);
			bStillRegistered = GCallbackRegistry.Contains(CallbackId);
		}
		if (!bStillRegistered)
		{
			return;
		}

		FString Message;
		if (!ParseSyncResponse(SyncResponse, Message))
		{
			{
				FScopeLock Lock(&GCallbackLock);
				GCallbackRegistry.Remove(CallbackId);
			}
			Env->DeleteGlobalRef(ProxyGlobal);
			UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] %s sync error: %s"), *MethodName, *Message);
			if (OnResult)
			{
				OnResult(true, BuildFailureEnvelope(Message));
			}
		}
	}

}

void TapBattleAndroidBridge::Init()
{
	if (GInitialized)
	{
		return;
	}
	GInitialized = true;

	FTapCoreAndroid::RegisterGlobalCallbackHandler([](int64 CallbackId, const FString& ResultJson) -> bool
	{
		{
			FScopeLock Lock(&GCallbackLock);
			if (!GCallbackRegistry.Contains(CallbackId))
			{
				return false;
			}
		}
		return DispatchCallback(CallbackId, ResultJson);
	});

	UE_LOG(LogTapBattle, Log, TEXT("[TapBattle][Android] Bridge initialized"));
}

void TapBattleAndroidBridge::SetLogLevel(int32 Level, bool bLogToConsole)
{
	const FString SyncResponse = DispatchSyncRoute(TEXT("setLogLevel"), BuildParamsWithBool(TEXT("level"), Level, TEXT("logToConsole"), bLogToConsole));
	FString Message;
	if (!ParseSyncResponse(SyncResponse, Message))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] setLogLevel failed: %s"), *Message);
	}
}

FString TapBattleAndroidBridge::GetVersion()
{
	const FString SyncResponse = DispatchSyncRoute(TEXT("getVersion"), TEXT(""));
	FString Message;
	if (!ParseSyncResponse(SyncResponse, Message))
	{
		UE_LOG(LogTapBattle, Verbose, TEXT("[TapBattle][Android] getVersion failed: %s"), *Message);
		return FString();
	}
	FString Version;
	return ParseSyncDataString(SyncResponse, Version) ? Version : FString();
}

int64 TapBattleAndroidBridge::NewRandomNumberGenerator(int32 Seed)
{
	const FString SyncResponse = DispatchSyncRoute(TEXT("newRandomNumberGenerator"), BuildParamsWithNumber(TEXT("seed"), Seed));
	FString Message;
	if (!ParseSyncResponse(SyncResponse, Message))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] newRandomNumberGenerator failed: %s"), *Message);
		return -1;
	}
	int64 ObjId = -1;
	return ParseSyncDataInt64(SyncResponse, ObjId) ? ObjId : -1;
}

int32 TapBattleAndroidBridge::RandomInt(int64 ObjId)
{
	const FString SyncResponse = DispatchSyncRoute(TEXT("randomInt"), BuildParamsWithString(TEXT("objID"), FString::Printf(TEXT("%lld"), ObjId)));
	FString Message;
	if (!ParseSyncResponse(SyncResponse, Message))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] randomInt failed: %s"), *Message);
		return 0;
	}
	int32 Value = 0;
	return ParseSyncDataInt32(SyncResponse, Value) ? Value : 0;
}

void TapBattleAndroidBridge::FreeRandomNumberGenerator(int64 ObjId)
{
	const FString SyncResponse = DispatchSyncRoute(TEXT("freeRandomNumberGenerator"), BuildParamsWithString(TEXT("objID"), FString::Printf(TEXT("%lld"), ObjId)));
	FString Message;
	if (!ParseSyncResponse(SyncResponse, Message))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] freeRandomNumberGenerator failed: %s"), *Message);
	}
}

bool TapBattleAndroidBridge::RegisterNotificationCallback(TFunction<void(const FString&)> OnNotification)
{
	JNIEnv* Env = FAndroidApplication::GetJavaEnv();
	if (Env == nullptr)
	{
		return false;
	}

	UnregisterNotificationCallback();

	int64 CallbackId = 0;
	{
		FScopeLock Lock(&GCallbackLock);
		CallbackId = FTapAndroidCallbackId::Allocate();
		FBattleCallbackEntry Entry;
		Entry.Notification = MoveTemp(OnNotification);
		Entry.bPersistent = true;
		GCallbackRegistry.Add(CallbackId, MoveTemp(Entry));
	}

	jobject ProxyGlobal = CreateProxyForId(Env, CallbackId);
	if (ProxyGlobal == nullptr)
	{
		FScopeLock Lock(&GCallbackLock);
		GCallbackRegistry.Remove(CallbackId);
		return false;
	}

	{
		FScopeLock Lock(&GCallbackLock);
		if (FBattleCallbackEntry* Entry = GCallbackRegistry.Find(CallbackId))
		{
			Entry->ProxyGlobal = ProxyGlobal;
		}
		GNotificationCallbackId = CallbackId;
	}

	const FString SyncResponse = InvokeRoute(Env, TEXT("registerOnlineBattleCallback"), TEXT(""), ProxyGlobal);
	FString Message;
	if (!ParseSyncResponse(SyncResponse, Message))
	{
		{
			FScopeLock Lock(&GCallbackLock);
			GCallbackRegistry.Remove(CallbackId);
			GNotificationCallbackId = 0;
		}
		Env->DeleteGlobalRef(ProxyGlobal);
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] register callback failed: %s"), *Message);
		return false;
	}

	UE_LOG(LogTapBattle, Log, TEXT("[TapBattle][Android] Notification callback registered, ID=%lld"), CallbackId);
	return true;
}

void TapBattleAndroidBridge::UnregisterNotificationCallback()
{
	int64 CallbackId = 0;
	jobject ProxyToRelease = nullptr;
	{
		FScopeLock Lock(&GCallbackLock);
		CallbackId = GNotificationCallbackId;
		if (CallbackId == 0)
		{
			return;
		}
		if (FBattleCallbackEntry* Entry = GCallbackRegistry.Find(CallbackId))
		{
			ProxyToRelease = Entry->ProxyGlobal;
		}
		GCallbackRegistry.Remove(CallbackId);
		GNotificationCallbackId = 0;
	}

	if (JNIEnv* Env = FAndroidApplication::GetJavaEnv())
	{
		const FString SyncResponse = InvokeRoute(Env, TEXT("unRegisterOnlineBattleCallback"), TEXT(""), nullptr);
		FString Message;
		if (!ParseSyncResponse(SyncResponse, Message))
		{
			UE_LOG(LogTapBattle, Verbose, TEXT("[TapBattle][Android] unregister callback returned: %s"), *Message);
		}
		if (ProxyToRelease != nullptr)
		{
			Env->DeleteGlobalRef(ProxyToRelease);
		}
	}
}

void TapBattleAndroidBridge::Connect(FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("connect"), TEXT(""), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::Disconnect(FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("disconnect"), TEXT(""), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::CreateRoom(const FString& ParamsJson, FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("createRoom"), ParamsJson, MoveTemp(OnResult));
}

void TapBattleAndroidBridge::MatchRoom(const FString& ParamsJson, FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("matchRoom"), ParamsJson, MoveTemp(OnResult));
}

void TapBattleAndroidBridge::GetRoomList(const FString& ParamsJson, FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("getRoomList"), ParamsJson, MoveTemp(OnResult));
}

void TapBattleAndroidBridge::JoinRoom(const FString& ParamsJson, FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("joinRoom"), ParamsJson, MoveTemp(OnResult));
}

void TapBattleAndroidBridge::LeaveRoom(FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("leaveRoom"), TEXT(""), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::UpdatePlayerCustomStatus(int32 Status, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetNumberField(TEXT("status"), Status);
	DispatchOneShotRoute(TEXT("updatePlayerCustomStatus"), SerializeObject(Params), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::UpdatePlayerCustomProperties(const FString& Properties, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("properties"), Properties);
	DispatchOneShotRoute(TEXT("updatePlayerCustomProperties"), SerializeObject(Params), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::UpdateRoomProperties(const FString& RoomName, const FString& Properties, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("name"), RoomName);
	Params->SetStringField(TEXT("customProperties"), Properties);
	DispatchOneShotRoute(TEXT("updateRoomProperties"), SerializeObject(Params), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::KickRoomPlayer(const FString& PlayerId, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("playerId"), PlayerId);
	DispatchOneShotRoute(TEXT("kickRoomPlayer"), SerializeObject(Params), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::StartFrameSync(FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("startFrameSync"), TEXT(""), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::SendFrameInput(const FString& Data, FResultCallback OnResult)
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("data"), Data);
	DispatchOneShotRoute(TEXT("sendFrameInput"), SerializeObject(Params), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::StopFrameSync(FResultCallback OnResult)
{
	DispatchOneShotRoute(TEXT("stopFrameSync"), TEXT(""), MoveTemp(OnResult));
}

void TapBattleAndroidBridge::SendCustomMessage(
	const FString& Msg,
	int32 Type,
	const TArray<FString>& Receivers,
	FResultCallback OnResult)
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(TEXT("msg"), Msg);
	Params->SetNumberField(TEXT("type"), Type);

	TArray<TSharedPtr<FJsonValue>> ReceiverArray;
	for (const FString& Receiver : Receivers)
	{
		ReceiverArray.Add(MakeShared<FJsonValueString>(Receiver));
	}
	Params->SetArrayField(TEXT("receivers"), ReceiverArray);

	DispatchOneShotRoute(TEXT("sendCustomMessage"), SerializeObject(Params), MoveTemp(OnResult));
}

#endif // PLATFORM_ANDROID

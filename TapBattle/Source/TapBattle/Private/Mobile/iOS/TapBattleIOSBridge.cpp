// Copyright TapTap. All Rights Reserved.

#include "TapBattleIOSBridge.h"

#if PLATFORM_IOS

#include "TapBattleModule.h"
#include "HAL/CriticalSection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

THIRD_PARTY_INCLUDES_START
#include "onlinebattle_sdk.h"
#include "onlinebattle_common.h"
THIRD_PARTY_INCLUDES_END

namespace
{
	constexpr int32 ResultSuccess = 1;

	struct FCallbackEntry
	{
		TapBattleIOSBridge::FResultCallback Callback;
	};

	FCriticalSection GCallbackLock;
	TMap<int64, FCallbackEntry> GCallbackRegistry;
	int64 GNextRequestId = 900000;
	OnlineBattleCallbacks GCallbacks{};
	bool GInitialized = false;
	TapBattleIOSBridge::FNotificationCallback GNotificationCallback;

	FString JsonObjectToString(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	FString BytesToString(const void* Data, uint32 DataLen)
	{
		if (Data == nullptr || DataLen == 0)
		{
			return FString();
		}
		FUTF8ToTCHAR Converted(static_cast<const ANSICHAR*>(Data), static_cast<int32>(DataLen));
		return FString(Converted.Length(), Converted.Get());
	}

	TSharedPtr<FJsonObject> ParseObject(const FString& Json)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (FJsonSerializer::Deserialize(Reader, Obj) && Obj.IsValid())
		{
			return Obj;
		}
		return nullptr;
	}

	FString WrapEnvelope(int32 Code, const FString& Content, const FString& Message = FString())
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("code"), Code);
		Root->SetStringField(TEXT("message"), Message);
		Root->SetStringField(TEXT("content"), Content);
		return JsonObjectToString(Root);
	}

	FString BuildFailureContent(int32 Code, const FString& Message)
	{
		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("code"), Code);
		Obj->SetStringField(TEXT("msg"), Message);
		return JsonObjectToString(Obj);
	}

	FString WrapRequestResult(int Success, const FString& Data)
	{
		return Success == ResultSuccess
			? WrapEnvelope(0, Data, TEXT("success"))
			: WrapEnvelope(-1, Data, TEXT("fail"));
	}

	FString BuildImmediateFailure(int32 ReturnCode, const TCHAR* Api)
	{
		return WrapEnvelope(
			-1,
			BuildFailureContent(ReturnCode, FString::Printf(TEXT("%s returned %d"), Api, ReturnCode)),
			TEXT("fail"));
	}

	int64 RegisterCallback(TapBattleIOSBridge::FResultCallback Callback)
	{
		FScopeLock Lock(&GCallbackLock);
		const int64 RequestId = GNextRequestId++;
		FCallbackEntry Entry;
		Entry.Callback = MoveTemp(Callback);
		GCallbackRegistry.Add(RequestId, MoveTemp(Entry));
		return RequestId;
	}

	TapBattleIOSBridge::FResultCallback TakeCallback(int64 RequestId)
	{
		FScopeLock Lock(&GCallbackLock);
		FCallbackEntry Entry;
		if (!GCallbackRegistry.RemoveAndCopyValue(RequestId, Entry))
		{
			return nullptr;
		}
		return MoveTemp(Entry.Callback);
	}

	void FailAndRemove(int64 RequestId, int32 ReturnCode, const TCHAR* Api)
	{
		TapBattleIOSBridge::FResultCallback Callback = TakeCallback(RequestId);
		if (Callback)
		{
			Callback(true, BuildImmediateFailure(ReturnCode, Api));
		}
	}

	void DispatchRequest(int64 RequestId, int Success, const void* Data, uint32 DataLen)
	{
		TapBattleIOSBridge::FResultCallback Callback = TakeCallback(RequestId);
		if (!Callback)
		{
			UE_LOG(LogTapBattle, Verbose, TEXT("[TapBattle][iOS] callback not found for request %lld"), RequestId);
			return;
		}
		Callback(true, WrapRequestResult(Success, BytesToString(Data, DataLen)));
	}

	TSharedRef<FJsonObject> BuildNotificationInner(const FString& EventName, const void* Data, uint32 DataLen)
	{
		TSharedRef<FJsonObject> Inner = MakeShared<FJsonObject>();
		Inner->SetStringField(TEXT("kind"), TEXT("event"));
		Inner->SetStringField(TEXT("event"), EventName);

		const FString DataJson = BytesToString(Data, DataLen);
		TSharedPtr<FJsonObject> DataObj = ParseObject(DataJson);
		if (DataObj.IsValid())
		{
			Inner->SetObjectField(TEXT("data"), DataObj);
		}
		else
		{
			Inner->SetObjectField(TEXT("data"), MakeShared<FJsonObject>());
		}
		return Inner;
	}

	void DispatchNotification(const FString& EventName, const void* Data, uint32 DataLen)
	{
		TapBattleIOSBridge::FNotificationCallback Callback;
		{
			FScopeLock Lock(&GCallbackLock);
			Callback = GNotificationCallback;
		}
		if (!Callback)
		{
			return;
		}

		Callback(WrapEnvelope(0, JsonObjectToString(BuildNotificationInner(EventName, Data, DataLen)), TEXT("success")));
	}

	void DispatchState(const FString& StateName, int32 RawCode, const FString& Message)
	{
		TapBattleIOSBridge::FNotificationCallback Callback;
		{
			FScopeLock Lock(&GCallbackLock);
			Callback = GNotificationCallback;
		}
		if (!Callback)
		{
			return;
		}

		TSharedRef<FJsonObject> Inner = MakeShared<FJsonObject>();
		Inner->SetStringField(TEXT("kind"), TEXT("state"));
		Inner->SetStringField(TEXT("state"), StateName);
		Inner->SetNumberField(TEXT("rawCode"), RawCode);
		if (!Message.IsEmpty())
		{
			TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
			Error->SetNumberField(TEXT("code"), RawCode);
			Error->SetStringField(TEXT("msg"), Message);
			Inner->SetObjectField(TEXT("error"), Error);
		}
		Callback(WrapEnvelope(0, JsonObjectToString(Inner), TEXT("success")));
	}

	void DispatchDisconnectNotification(const void* Data, uint32 DataLen)
	{
		const FString ErrorJson = BytesToString(Data, DataLen);
		TSharedPtr<FJsonObject> ErrorObj = ParseObject(ErrorJson);
		int32 Code = 0;
		FString Message;
		if (ErrorObj.IsValid())
		{
			double CodeNumber = 0.0;
			if (ErrorObj->TryGetNumberField(TEXT("code"), CodeNumber))
			{
				Code = static_cast<int32>(CodeNumber);
			}
			ErrorObj->TryGetStringField(TEXT("msg"), Message);
		}
		DispatchState(TEXT("disconnected"), Code, Message);
	}

	void OnRequest(int64_t ReqId, const char* /*ClientId*/, int Success, const void* Data, uint32_t DataLen)
	{
		DispatchRequest(static_cast<int64>(ReqId), Success, Data, DataLen);
	}

	void OnDisconnect(int64_t ReqId, const char* /*ClientId*/)
	{
		TapBattleIOSBridge::FResultCallback Callback = TakeCallback(static_cast<int64>(ReqId));
		if (Callback)
		{
			Callback(true, WrapEnvelope(0, FString(), TEXT("success")));
		}
	}

	void OnEnterRoomNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("playerEntered"), Data, DataLen); }
	void OnLeaveRoomNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("playerLeft"), Data, DataLen); }
	void OnPlayerOfflineNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("playerOffline"), Data, DataLen); }
	void OnPlayerCustomStatusNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("playerCustomStatusChanged"), Data, DataLen); }
	void OnPlayerCustomPropertiesNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("playerCustomPropertiesChanged"), Data, DataLen); }
	void OnRoomPropertiesNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("roomPropertiesChanged"), Data, DataLen); }
	void OnRoomPlayerKickedNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("playerKicked"), Data, DataLen); }
	void OnFrameSyncStartNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("frameSyncStarted"), Data, DataLen); }
	void OnFrameSyncNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("frameReceived"), Data, DataLen); }
	void OnCustomMessageNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("customMessageReceived"), Data, DataLen); }
	void OnFrameSyncStopNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchNotification(TEXT("frameSyncStopped"), Data, DataLen); }
	void OnBattleServiceError(const char* /*ClientId*/, const void* /*Data*/, uint32_t /*DataLen*/) { DispatchState(TEXT("battleServiceError"), 0, FString()); }
	void OnDisconnectNotification(const char* /*ClientId*/, const void* Data, uint32_t DataLen) { DispatchDisconnectNotification(Data, DataLen); }

	// 原生 onlinebattle_sdk 内部日志回调。未注册时（旧实现）原生内部的失败原因
	// （如 dataDir 不可写、缓存目录创建失败等）会被整个吞掉，UE 侧只看到笼统的
	// Initialize -1。注册后原生会把每条内部日志透传到 LogTapBattle。与 Desktop 对齐。
	//
	// TapSdkOnlineBattleInitLogger 仅在 onlinebattle_sdk.h 的 #ifdef TAPSDK_SHARED_LIB
	// 下声明，且静态 xcframework（iOS 链接的产物）未导出该符号(nm 验证)。故仅在
	// shared-lib 构建下编译回调注册；iOS 静态库改用 SetLogLevel 把原生日志输出到设备
	// console（见下方 InitializeNative）。
#if defined(TAPSDK_SHARED_LIB)
	void OnNativeLog(int32_t NativeLogLevel, const char* CodeLocation, const char* LogTag, const char* LogMessage)
	{
		const FString Message = FString::Printf(
			TEXT("[TapSDK-%s] [%s] %s"),
			UTF8_TO_TCHAR(LogTag ? LogTag : ""),
			UTF8_TO_TCHAR(CodeLocation ? CodeLocation : ""),
			UTF8_TO_TCHAR(LogMessage ? LogMessage : ""));
		if (NativeLogLevel <= 3)
		{
			UE_LOG(LogTapBattle, Log, TEXT("%s"), *Message);
		}
		else if (NativeLogLevel == 4)
		{
			UE_LOG(LogTapBattle, Warning, TEXT("%s"), *Message);
		}
		else
		{
			UE_LOG(LogTapBattle, Error, TEXT("%s"), *Message);
		}
	}
#endif // TAPSDK_SHARED_LIB

	OnlineBattleCallbacks BuildCallbacks()
	{
		OnlineBattleCallbacks Callbacks{};
		Callbacks.ConnectCB = &OnRequest;
		Callbacks.DisconnectCB = &OnDisconnect;
		Callbacks.CreateRoomCB = &OnRequest;
		Callbacks.MatchRoomCB = &OnRequest;
		Callbacks.GetRoomListCB = &OnRequest;
		Callbacks.JoinRoomCB = &OnRequest;
		Callbacks.LeaveRoomCB = &OnRequest;
		Callbacks.UpdatePlayerCustomStatusCB = &OnRequest;
		Callbacks.UpdatePlayerCustomPropertiesCB = &OnRequest;
		Callbacks.UpdateRoomPropertiesCB = &OnRequest;
		Callbacks.KickRoomPlayerCB = &OnRequest;
		Callbacks.StartFrameSyncCB = &OnRequest;
		Callbacks.SendFrameInputCB = &OnRequest;
		Callbacks.SendCustomMessageCB = &OnRequest;
		Callbacks.StopFrameSyncCB = &OnRequest;

		Callbacks.EnterRoomNotificationCB = &OnEnterRoomNotification;
		Callbacks.LeaveRoomNotificationCB = &OnLeaveRoomNotification;
		Callbacks.PlayerOfflineNotificationCB = &OnPlayerOfflineNotification;
		Callbacks.PlayerCustomStatusNotificationCB = &OnPlayerCustomStatusNotification;
		Callbacks.PlayerCustomPropertiesNotificationCB = &OnPlayerCustomPropertiesNotification;
		Callbacks.RoomPropertiesNotificationCB = &OnRoomPropertiesNotification;
		Callbacks.RoomPlayerKickedNotificationCB = &OnRoomPlayerKickedNotification;
		Callbacks.FrameSyncStartNotificationCB = &OnFrameSyncStartNotification;
		Callbacks.FrameSyncCB = &OnFrameSyncNotification;
		Callbacks.CustomMessageNotificationCB = &OnCustomMessageNotification;
		Callbacks.FrameSyncStopNotificationCB = &OnFrameSyncStopNotification;
		Callbacks.BattleServiceErrorCB = &OnBattleServiceError;
		Callbacks.DisconnectNotificationCB = &OnDisconnectNotification;
		return Callbacks;
	}

	void DispatchWithStringPayload(
		const FString& ClientId,
		const FString& Payload,
		const TCHAR* Api,
		TapBattleIOSBridge::FResultCallback OnResult,
		TFunction<int(int64_t, const char*, const void*, uint32_t)> NativeCall)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		FTCHARToUTF8 PayloadUtf8(*Payload);
		const int ReturnCode = NativeCall(
			static_cast<int64_t>(RequestId),
			ClientUtf8.Get(),
			PayloadUtf8.Get(),
			static_cast<uint32_t>(PayloadUtf8.Length()));
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, Api);
		}
	}
}

namespace TapBattleIOSBridge
{
	bool Init(const FString& ConfigJson, FNotificationCallback OnNotification)
	{
		{
			FScopeLock Lock(&GCallbackLock);
			GNotificationCallback = MoveTemp(OnNotification);
		}

		if (GInitialized)
		{
			return true;
		}

		GCallbacks = BuildCallbacks();
		// 必须在 Initialize 之前打开日志级别，否则初始化过程的原生内部日志（-1 的真正
		// 原因）拿不到。SetLogLevel 静态库已导出；InitLogger 仅 shared-lib 有，iOS 静态
		// 库走 logToConsole=1 把原生日志输出到设备 console（Console.app/idevicesyslog）。
#if defined(TAPSDK_SHARED_LIB)
		TapSdkOnlineBattleInitLogger(&OnNativeLog);
#endif
		// 仅调试构建强制 trace+console，抓 init 过程本身的原生日志；Shipping 不强制，
		// 回归 config 里的 logLevel/logToConsole（由 Initialize 读取），避免正式包刷
		// 海量日志、泄露内部信息或拖性能。
#if !UE_BUILD_SHIPPING
		TapSdkOnlineBattleSetLogLevel(1, 1); // 1 trace + 输出到 console
#endif
		FTCHARToUTF8 ConfigUtf8(*ConfigJson);
		const int Result = TapSdkOnlineBattleInitialize(ConfigUtf8.Get(), &GCallbacks);
		GInitialized = (Result == 0);
		if (!GInitialized)
		{
			UE_LOG(LogTapBattle, Error, TEXT("[TapBattle][iOS] TapSdkOnlineBattleInitialize failed: %d"), Result);
		}
		return GInitialized;
	}

	bool IsReady()
	{
		return GInitialized;
	}

	void Finalize()
	{
		if (GInitialized)
		{
			TapSdkOnlineBattleFinalize();
			GInitialized = false;
		}
		{
			FScopeLock Lock(&GCallbackLock);
			GCallbackRegistry.Empty();
			GCallbacks = OnlineBattleCallbacks{};
			GNotificationCallback = nullptr;
		}
	}

	void SetLogLevel(int32 Level, bool bLogToConsole)
	{
		TapSdkOnlineBattleSetLogLevel(Level, bLogToConsole ? 1 : 0);
	}

	FString GetVersion()
	{
		const char* Version = TapSdkOnlineBattleVersion();
		return Version ? FString(UTF8_TO_TCHAR(Version)) : FString();
	}

	int64 NewRandomNumberGenerator(int32 Seed)
	{
		return static_cast<int64>(TapSdkOnlineBattleNewRandomNumberGenerator(Seed));
	}

	int32 RandomInt(int64 ObjId)
	{
		return TapSdkOnlineBattleRandomInt(static_cast<int64_t>(ObjId));
	}

	void FreeRandomNumberGenerator(int64 ObjId)
	{
		TapSdkOnlineBattleFreeRandomNumberGenerator(static_cast<int64_t>(ObjId));
	}

	void Connect(const FString& ConfigJson, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ConfigUtf8(*ConfigJson);
		const int ReturnCode = TapSdkOnlineBattleConnect(static_cast<int64_t>(RequestId), ConfigUtf8.Get());
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("connect"));
		}
	}

	void Disconnect(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = TapSdkOnlineBattleDisconnect(static_cast<int64_t>(RequestId), ClientUtf8.Get());
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("disconnect"));
		}
	}

	void CreateRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("createRoom"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return TapSdkOnlineBattleCreateRoom(Id, Client, Data, Len); });
	}

	void MatchRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("matchRoom"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return TapSdkOnlineBattleMatchRoom(Id, Client, Data, Len); });
	}

	void GetRoomList(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("getRoomList"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return TapSdkOnlineBattleGetRoomList(Id, Client, Data, Len); });
	}

	void JoinRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("joinRoom"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return TapSdkOnlineBattleJoinRoom(Id, Client, Data, Len); });
	}

	void LeaveRoom(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = TapSdkOnlineBattleLeaveRoom(static_cast<int64_t>(RequestId), ClientUtf8.Get());
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("leaveRoom"));
		}
	}

	void UpdatePlayerCustomStatus(const FString& ClientId, int32 Status, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = TapSdkOnlineBattleUpdatePlayerCustomStatus(static_cast<int64_t>(RequestId), ClientUtf8.Get(), Status);
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("updatePlayerCustomStatus"));
		}
	}

	void UpdatePlayerCustomProperties(const FString& ClientId, const FString& Properties, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		FTCHARToUTF8 PropsUtf8(*Properties);
		const int ReturnCode = TapSdkOnlineBattleUpdatePlayerCustomProperties(static_cast<int64_t>(RequestId), ClientUtf8.Get(), PropsUtf8.Get());
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("updatePlayerCustomProperties"));
		}
	}

	void UpdateRoomProperties(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("updateRoomProperties"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return TapSdkOnlineBattleUpdateRoomProperties(Id, Client, Data, Len); });
	}

	void KickRoomPlayer(const FString& ClientId, const FString& PlayerId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		FTCHARToUTF8 PlayerUtf8(*PlayerId);
		const int ReturnCode = TapSdkOnlineBattleKickRoomPlayer(static_cast<int64_t>(RequestId), ClientUtf8.Get(), PlayerUtf8.Get());
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("kickRoomPlayer"));
		}
	}

	void StartFrameSync(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = TapSdkOnlineBattleStartFrameSync(static_cast<int64_t>(RequestId), ClientUtf8.Get());
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("startFrameSync"));
		}
	}

	void SendFrameInput(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("sendFrameInput"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return TapSdkOnlineBattleSendFrameInput(Id, Client, Data, Len); });
	}

	void StopFrameSync(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = TapSdkOnlineBattleStopFrameSync(static_cast<int64_t>(RequestId), ClientUtf8.Get());
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("stopFrameSync"));
		}
	}

	void SendCustomMessage(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("sendCustomMessage"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return TapSdkOnlineBattleSendCustomMessage(Id, Client, Data, Len); });
	}
}

#endif // PLATFORM_IOS

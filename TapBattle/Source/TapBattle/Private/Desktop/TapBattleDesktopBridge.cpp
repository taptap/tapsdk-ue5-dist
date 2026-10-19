// Copyright TapTap. All Rights Reserved.

#include "Desktop/TapBattleDesktopBridge.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapBattleModule.h"
#include "HAL/CriticalSection.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	constexpr int32 ResultSuccess = 1;

	typedef void (*RequestCallback)(int64_t ReqId, const char* ClientId, int Success, const void* Data, uint32_t DataLen);
	typedef void (*DisconnectCallback)(int64_t ReqId, const char* ClientId);
	typedef void (*NotificationCallback)(const char* ClientId, const void* Data, uint32_t DataLen);
	typedef void (*TapSdkCppLogWriter)(int LogLevel, const char* CodeLocation, const char* LogTag, const char* LogMessage);

	struct OnlineBattleCallbacks
	{
		RequestCallback ConnectCB = nullptr;
		DisconnectCallback DisconnectCB = nullptr;
		RequestCallback CreateRoomCB = nullptr;
		RequestCallback MatchRoomCB = nullptr;
		RequestCallback GetRoomListCB = nullptr;
		RequestCallback JoinRoomCB = nullptr;
		RequestCallback LeaveRoomCB = nullptr;
		RequestCallback UpdatePlayerCustomStatusCB = nullptr;
		RequestCallback UpdatePlayerCustomPropertiesCB = nullptr;
		RequestCallback UpdateRoomPropertiesCB = nullptr;
		RequestCallback KickRoomPlayerCB = nullptr;
		RequestCallback StartFrameSyncCB = nullptr;
		RequestCallback SendFrameInputCB = nullptr;
		RequestCallback SendCustomMessageCB = nullptr;
		RequestCallback StopFrameSyncCB = nullptr;

		NotificationCallback EnterRoomNotificationCB = nullptr;
		NotificationCallback LeaveRoomNotificationCB = nullptr;
		NotificationCallback PlayerOfflineNotificationCB = nullptr;
		NotificationCallback PlayerCustomStatusNotificationCB = nullptr;
		NotificationCallback PlayerCustomPropertiesNotificationCB = nullptr;
		NotificationCallback RoomPropertiesNotificationCB = nullptr;
		NotificationCallback RoomPlayerKickedNotificationCB = nullptr;
		NotificationCallback FrameSyncStartNotificationCB = nullptr;
		NotificationCallback FrameSyncCB = nullptr;
		NotificationCallback CustomMessageNotificationCB = nullptr;
		NotificationCallback FrameSyncStopNotificationCB = nullptr;
		NotificationCallback BattleServiceErrorCB = nullptr;
		NotificationCallback DisconnectNotificationCB = nullptr;
	};

	typedef void (*FnInitLogger)(TapSdkCppLogWriter LogWriter);
	typedef int (*FnInitialize)(const char* JsonCfg, const OnlineBattleCallbacks* Callbacks);
	typedef void (*FnFinalize)();
	typedef int64_t (*FnNewRandomNumberGenerator)(int Seed);
	typedef int (*FnRandomInt)(int64_t ObjId);
	typedef void (*FnFreeRandomNumberGenerator)(int64_t ObjId);
	typedef void (*FnSetLogLevel)(int LogLevel, int LogToConsole);
	typedef const char* (*FnStringGetter)();
	typedef int (*FnConnect)(int64_t ReqId, const char* JsonCfg);
	typedef int (*FnDisconnect)(int64_t ReqId, const char* ClientId);
	typedef int (*FnPayloadRequest)(int64_t ReqId, const char* ClientId, const void* Data, uint32_t DataLen);
	typedef int (*FnStatusRequest)(int64_t ReqId, const char* ClientId, int32_t Status);
	typedef int (*FnStringRequest)(int64_t ReqId, const char* ClientId, const char* Value);
	typedef int (*FnSimpleRequest)(int64_t ReqId, const char* ClientId);

	void* GDllHandle = nullptr;
	FnInitLogger PfnInitLogger = nullptr;
	FnInitialize PfnInitialize = nullptr;
	FnFinalize PfnFinalize = nullptr;
	FnNewRandomNumberGenerator PfnNewRandomNumberGenerator = nullptr;
	FnRandomInt PfnRandomInt = nullptr;
	FnFreeRandomNumberGenerator PfnFreeRandomNumberGenerator = nullptr;
	FnSetLogLevel PfnSetLogLevel = nullptr;
	FnStringGetter PfnVersion = nullptr;
	FnStringGetter PfnGitCommit = nullptr;
	FnConnect PfnConnect = nullptr;
	FnDisconnect PfnDisconnect = nullptr;
	FnPayloadRequest PfnCreateRoom = nullptr;
	FnPayloadRequest PfnMatchRoom = nullptr;
	FnPayloadRequest PfnGetRoomList = nullptr;
	FnPayloadRequest PfnJoinRoom = nullptr;
	FnSimpleRequest PfnLeaveRoom = nullptr;
	FnStatusRequest PfnUpdatePlayerCustomStatus = nullptr;
	FnStringRequest PfnUpdatePlayerCustomProperties = nullptr;
	FnPayloadRequest PfnUpdateRoomProperties = nullptr;
	FnStringRequest PfnKickRoomPlayer = nullptr;
	FnSimpleRequest PfnStartFrameSync = nullptr;
	FnPayloadRequest PfnSendFrameInput = nullptr;
	FnPayloadRequest PfnSendCustomMessage = nullptr;
	FnSimpleRequest PfnStopFrameSync = nullptr;

	FCriticalSection GCallbackLock;
	TMap<int64, TapBattleDesktopBridge::FResultCallback> GCallbackRegistry;
	int64 GNextRequestId = 800000;
	OnlineBattleCallbacks GCallbacks{};
	bool GInitialized = false;
	TapBattleDesktopBridge::FNotificationCallback GNotificationCallback;

	FString DllName()
	{
#if PLATFORM_WINDOWS
		return TEXT("onlinebattle_sdk.dll");
#elif PLATFORM_MAC
		return TEXT("libonlinebattle_sdk.dylib");
#else
		return TEXT("libonlinebattle_sdk.so");
#endif
	}

	FString DllAbsPath()
	{
		const FString Plugin = FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("TapBattle"));
#if PLATFORM_WINDOWS
		return FPaths::Combine(Plugin, TEXT("Source/TapBattle/x64"), DllName());
#elif PLATFORM_MAC
		return FPaths::Combine(Plugin, TEXT("Source/TapBattle"), DllName());
#else
		return FPaths::Combine(Plugin, TEXT("Source/TapBattle"), DllName());
#endif
	}

	template <typename T>
	T ResolveRequired(const char* Symbol)
	{
		void* Sym = FPlatformProcess::GetDllExport(GDllHandle, ANSI_TO_TCHAR(Symbol));
		if (!Sym)
		{
			UE_LOG(LogTapBattle, Error, TEXT("Failed to resolve onlinebattle_sdk symbol: %s"), ANSI_TO_TCHAR(Symbol));
		}
		return reinterpret_cast<T>(Sym);
	}

	template <typename T>
	T ResolveOptional(const char* Symbol)
	{
		void* Sym = FPlatformProcess::GetDllExport(GDllHandle, ANSI_TO_TCHAR(Symbol));
		return reinterpret_cast<T>(Sym);
	}

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

	int64 RegisterCallback(TapBattleDesktopBridge::FResultCallback Callback)
	{
		FScopeLock Lock(&GCallbackLock);
		const int64 RequestId = GNextRequestId++;
		GCallbackRegistry.Add(RequestId, MoveTemp(Callback));
		return RequestId;
	}

	TapBattleDesktopBridge::FResultCallback TakeCallback(int64 RequestId)
	{
		FScopeLock Lock(&GCallbackLock);
		TapBattleDesktopBridge::FResultCallback Callback;
		if (!GCallbackRegistry.RemoveAndCopyValue(RequestId, Callback))
		{
			return nullptr;
		}
		return MoveTemp(Callback);
	}

	void FailAndRemove(int64 RequestId, int32 ReturnCode, const TCHAR* Api)
	{
		TapBattleDesktopBridge::FResultCallback Callback = TakeCallback(RequestId);
		if (Callback)
		{
			Callback(true, BuildImmediateFailure(ReturnCode, Api));
		}
	}

	void DispatchRequest(int64 RequestId, int Success, const void* Data, uint32 DataLen)
	{
		TapBattleDesktopBridge::FResultCallback Callback = TakeCallback(RequestId);
		if (!Callback)
		{
			UE_LOG(LogTapBattle, Verbose, TEXT("[TapBattle][Desktop] callback not found for request %lld"), RequestId);
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
		TapBattleDesktopBridge::FNotificationCallback Callback;
		{
			FScopeLock Lock(&GCallbackLock);
			Callback = GNotificationCallback;
		}
		if (Callback)
		{
			Callback(WrapEnvelope(0, JsonObjectToString(BuildNotificationInner(EventName, Data, DataLen)), TEXT("success")));
		}
	}

	void DispatchState(const FString& StateName, int32 RawCode, const FString& Message)
	{
		TapBattleDesktopBridge::FNotificationCallback Callback;
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

	void OnLog(int NativeLogLevel, const char* CodeLocation, const char* LogTag, const char* LogMessage)
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

	void OnRequest(int64_t ReqId, const char* /*ClientId*/, int Success, const void* Data, uint32_t DataLen)
	{
		DispatchRequest(static_cast<int64>(ReqId), Success, Data, DataLen);
	}

	void OnDisconnect(int64_t ReqId, const char* /*ClientId*/)
	{
		TapBattleDesktopBridge::FResultCallback Callback = TakeCallback(static_cast<int64>(ReqId));
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

	bool HasRequiredSymbols()
	{
		return PfnInitialize && PfnFinalize && PfnNewRandomNumberGenerator && PfnRandomInt && PfnFreeRandomNumberGenerator &&
			PfnSetLogLevel && PfnConnect && PfnDisconnect && PfnCreateRoom && PfnMatchRoom && PfnGetRoomList && PfnJoinRoom &&
			PfnLeaveRoom && PfnUpdatePlayerCustomStatus && PfnUpdatePlayerCustomProperties && PfnUpdateRoomProperties && PfnKickRoomPlayer &&
			PfnStartFrameSync && PfnSendFrameInput && PfnSendCustomMessage && PfnStopFrameSync;
	}

	bool LoadLibraryAndSymbols()
	{
		if (GDllHandle && HasRequiredSymbols())
		{
			return true;
		}

		const FString ShortName = DllName();
		if (!ShortName.IsEmpty())
		{
			GDllHandle = FPlatformProcess::GetDllHandle(*ShortName);
		}

		FString AbsPath;
		if (!GDllHandle)
		{
			AbsPath = DllAbsPath();
			GDllHandle = FPlatformProcess::GetDllHandle(*AbsPath);
		}

		if (!GDllHandle)
		{
			UE_LOG(LogTapBattle, Error, TEXT("onlinebattle_sdk native library failed to load (short=%s, abs=%s)"), *ShortName, *AbsPath);
			return false;
		}

		PfnInitLogger = ResolveOptional<FnInitLogger>("TapSdkOnlineBattleInitLogger");
		PfnInitialize = ResolveRequired<FnInitialize>("TapSdkOnlineBattleInitialize");
		PfnFinalize = ResolveRequired<FnFinalize>("TapSdkOnlineBattleFinalize");
		PfnNewRandomNumberGenerator = ResolveRequired<FnNewRandomNumberGenerator>("TapSdkOnlineBattleNewRandomNumberGenerator");
		PfnRandomInt = ResolveRequired<FnRandomInt>("TapSdkOnlineBattleRandomInt");
		PfnFreeRandomNumberGenerator = ResolveRequired<FnFreeRandomNumberGenerator>("TapSdkOnlineBattleFreeRandomNumberGenerator");
		PfnSetLogLevel = ResolveRequired<FnSetLogLevel>("TapSdkOnlineBattleSetLogLevel");
		PfnVersion = ResolveOptional<FnStringGetter>("TapSdkOnlineBattleVersion");
		PfnGitCommit = ResolveOptional<FnStringGetter>("TapSdkOnlineBattleGitCommit");
		PfnConnect = ResolveRequired<FnConnect>("TapSdkOnlineBattleConnect");
		PfnDisconnect = ResolveRequired<FnDisconnect>("TapSdkOnlineBattleDisconnect");
		PfnCreateRoom = ResolveRequired<FnPayloadRequest>("TapSdkOnlineBattleCreateRoom");
		PfnMatchRoom = ResolveRequired<FnPayloadRequest>("TapSdkOnlineBattleMatchRoom");
		PfnGetRoomList = ResolveRequired<FnPayloadRequest>("TapSdkOnlineBattleGetRoomList");
		PfnJoinRoom = ResolveRequired<FnPayloadRequest>("TapSdkOnlineBattleJoinRoom");
		PfnLeaveRoom = ResolveRequired<FnSimpleRequest>("TapSdkOnlineBattleLeaveRoom");
		PfnUpdatePlayerCustomStatus = ResolveRequired<FnStatusRequest>("TapSdkOnlineBattleUpdatePlayerCustomStatus");
		PfnUpdatePlayerCustomProperties = ResolveRequired<FnStringRequest>("TapSdkOnlineBattleUpdatePlayerCustomProperties");
		PfnUpdateRoomProperties = ResolveRequired<FnPayloadRequest>("TapSdkOnlineBattleUpdateRoomProperties");
		PfnKickRoomPlayer = ResolveRequired<FnStringRequest>("TapSdkOnlineBattleKickRoomPlayer");
		PfnStartFrameSync = ResolveRequired<FnSimpleRequest>("TapSdkOnlineBattleStartFrameSync");
		PfnSendFrameInput = ResolveRequired<FnPayloadRequest>("TapSdkOnlineBattleSendFrameInput");
		PfnSendCustomMessage = ResolveRequired<FnPayloadRequest>("TapSdkOnlineBattleSendCustomMessage");
		PfnStopFrameSync = ResolveRequired<FnSimpleRequest>("TapSdkOnlineBattleStopFrameSync");

		const bool bReady = HasRequiredSymbols();
		if (!bReady)
		{
			UE_LOG(LogTapBattle, Error, TEXT("onlinebattle_sdk native library loaded but required symbols are missing"));
		}
		return bReady;
	}

	void ResetSymbols()
	{
		PfnInitLogger = nullptr;
		PfnInitialize = nullptr;
		PfnFinalize = nullptr;
		PfnNewRandomNumberGenerator = nullptr;
		PfnRandomInt = nullptr;
		PfnFreeRandomNumberGenerator = nullptr;
		PfnSetLogLevel = nullptr;
		PfnVersion = nullptr;
		PfnGitCommit = nullptr;
		PfnConnect = nullptr;
		PfnDisconnect = nullptr;
		PfnCreateRoom = nullptr;
		PfnMatchRoom = nullptr;
		PfnGetRoomList = nullptr;
		PfnJoinRoom = nullptr;
		PfnLeaveRoom = nullptr;
		PfnUpdatePlayerCustomStatus = nullptr;
		PfnUpdatePlayerCustomProperties = nullptr;
		PfnUpdateRoomProperties = nullptr;
		PfnKickRoomPlayer = nullptr;
		PfnStartFrameSync = nullptr;
		PfnSendFrameInput = nullptr;
		PfnSendCustomMessage = nullptr;
		PfnStopFrameSync = nullptr;
	}

	void DispatchWithStringPayload(
		const FString& ClientId,
		const FString& Payload,
		const TCHAR* Api,
		TapBattleDesktopBridge::FResultCallback OnResult,
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

namespace TapBattleDesktopBridge
{
	bool IsReady()
	{
		return GDllHandle != nullptr && PfnInitialize != nullptr;
	}

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

		if (!LoadLibraryAndSymbols())
		{
			return false;
		}

		if (PfnInitLogger)
		{
			PfnInitLogger(&OnLog);
		}

		GCallbacks = BuildCallbacks();
		FTCHARToUTF8 ConfigUtf8(*ConfigJson);
		const int Result = PfnInitialize(ConfigUtf8.Get(), &GCallbacks);
		GInitialized = (Result == 0);
		if (!GInitialized)
		{
			UE_LOG(LogTapBattle, Error, TEXT("[TapBattle][Desktop] TapSdkOnlineBattleInitialize failed: %d"), Result);
		}
		return GInitialized;
	}

	void Finalize()
	{
		if (GInitialized && PfnFinalize)
		{
			PfnFinalize();
			GInitialized = false;
		}
		{
			FScopeLock Lock(&GCallbackLock);
			GCallbackRegistry.Empty();
			GCallbacks = OnlineBattleCallbacks{};
			GNotificationCallback = nullptr;
		}
		ResetSymbols();
		if (GDllHandle)
		{
			FPlatformProcess::FreeDllHandle(GDllHandle);
			GDllHandle = nullptr;
		}
	}

	void SetLogLevel(int32 Level, bool bLogToConsole)
	{
		if (PfnSetLogLevel)
		{
			PfnSetLogLevel(Level, bLogToConsole ? 1 : 0);
		}
	}

	FString GetVersion()
	{
		if (!PfnVersion)
		{
			LoadLibraryAndSymbols();
		}
		const char* Version = PfnVersion ? PfnVersion() : nullptr;
		return Version ? FString(UTF8_TO_TCHAR(Version)) : FString();
	}

	FString GetGitCommit()
	{
		if (!PfnGitCommit)
		{
			LoadLibraryAndSymbols();
		}
		const char* Commit = PfnGitCommit ? PfnGitCommit() : nullptr;
		return Commit ? FString(UTF8_TO_TCHAR(Commit)) : FString();
	}

	int64 NewRandomNumberGenerator(int32 Seed)
	{
		return PfnNewRandomNumberGenerator ? static_cast<int64>(PfnNewRandomNumberGenerator(Seed)) : -1;
	}

	int32 RandomInt(int64 ObjId)
	{
		return PfnRandomInt ? PfnRandomInt(static_cast<int64_t>(ObjId)) : 0;
	}

	void FreeRandomNumberGenerator(int64 ObjId)
	{
		if (PfnFreeRandomNumberGenerator)
		{
			PfnFreeRandomNumberGenerator(static_cast<int64_t>(ObjId));
		}
	}

	void Connect(const FString& ConfigJson, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ConfigUtf8(*ConfigJson);
		const int ReturnCode = PfnConnect ? PfnConnect(static_cast<int64_t>(RequestId), ConfigUtf8.Get()) : -2;
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("connect"));
		}
	}

	void Disconnect(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = PfnDisconnect ? PfnDisconnect(static_cast<int64_t>(RequestId), ClientUtf8.Get()) : -2;
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("disconnect"));
		}
	}

	void CreateRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("createRoom"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return PfnCreateRoom ? PfnCreateRoom(Id, Client, Data, Len) : -2; });
	}

	void MatchRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("matchRoom"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return PfnMatchRoom ? PfnMatchRoom(Id, Client, Data, Len) : -2; });
	}

	void GetRoomList(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("getRoomList"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return PfnGetRoomList ? PfnGetRoomList(Id, Client, Data, Len) : -2; });
	}

	void JoinRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("joinRoom"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return PfnJoinRoom ? PfnJoinRoom(Id, Client, Data, Len) : -2; });
	}

	void LeaveRoom(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = PfnLeaveRoom ? PfnLeaveRoom(static_cast<int64_t>(RequestId), ClientUtf8.Get()) : -2;
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("leaveRoom"));
		}
	}

	void UpdatePlayerCustomStatus(const FString& ClientId, int32 Status, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = PfnUpdatePlayerCustomStatus ? PfnUpdatePlayerCustomStatus(static_cast<int64_t>(RequestId), ClientUtf8.Get(), Status) : -2;
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
		const int ReturnCode = PfnUpdatePlayerCustomProperties ? PfnUpdatePlayerCustomProperties(static_cast<int64_t>(RequestId), ClientUtf8.Get(), PropsUtf8.Get()) : -2;
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("updatePlayerCustomProperties"));
		}
	}

	void UpdateRoomProperties(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("updateRoomProperties"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return PfnUpdateRoomProperties ? PfnUpdateRoomProperties(Id, Client, Data, Len) : -2; });
	}

	void KickRoomPlayer(const FString& ClientId, const FString& PlayerId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		FTCHARToUTF8 PlayerUtf8(*PlayerId);
		const int ReturnCode = PfnKickRoomPlayer ? PfnKickRoomPlayer(static_cast<int64_t>(RequestId), ClientUtf8.Get(), PlayerUtf8.Get()) : -2;
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("kickRoomPlayer"));
		}
	}

	void StartFrameSync(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = PfnStartFrameSync ? PfnStartFrameSync(static_cast<int64_t>(RequestId), ClientUtf8.Get()) : -2;
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("startFrameSync"));
		}
	}

	void SendFrameInput(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("sendFrameInput"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return PfnSendFrameInput ? PfnSendFrameInput(Id, Client, Data, Len) : -2; });
	}

	void StopFrameSync(const FString& ClientId, FResultCallback OnResult)
	{
		const int64 RequestId = RegisterCallback(MoveTemp(OnResult));
		FTCHARToUTF8 ClientUtf8(*ClientId);
		const int ReturnCode = PfnStopFrameSync ? PfnStopFrameSync(static_cast<int64_t>(RequestId), ClientUtf8.Get()) : -2;
		if (ReturnCode != 0)
		{
			FailAndRemove(RequestId, ReturnCode, TEXT("stopFrameSync"));
		}
	}

	void SendCustomMessage(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult)
	{
		DispatchWithStringPayload(ClientId, ParamsJson, TEXT("sendCustomMessage"), MoveTemp(OnResult),
			[](int64_t Id, const char* Client, const void* Data, uint32_t Len) { return PfnSendCustomMessage ? PfnSendCustomMessage(Id, Client, Data, Len) : -2; });
	}
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

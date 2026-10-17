// Copyright TapTap. All Rights Reserved.

#include "Platform/TapBattlePlatformDesktop.h"

#include "Desktop/TapBattleDesktopBridge.h"
#include "TapBattle.h"
#include "TapBattleModule.h"
#include "TapOnlineBattleCallbackManager.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "TapLogin.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "TapJsonCompat.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "HAL/PlatformMisc.h"
#if PLATFORM_WINDOWS
#include "Launcher/TapLauncher.h"
#endif
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

namespace
{
	void RunOnGameThread(TFunction<void()> Fn)
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(Fn));
	}

	struct FBridgeEnvelope
	{
		int32 Code = -1;
		FString Message;
		FString Content;
	};

	FString JsonObjectToString(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	bool ParseJsonObject(const FString& Json, TSharedPtr<FJsonObject>& Out)
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		return FJsonSerializer::Deserialize(Reader, Out) && Out.IsValid();
	}

	TSharedPtr<FJsonObject> GetObjectField(const TSharedPtr<FJsonObject>& Root, const FString& Field)
	{
		if (!Root.IsValid())
		{
			return nullptr;
		}
		const TSharedPtr<FJsonValue> Value = TapJson::FindField(Root, Field);
		if (!Value.IsValid())
		{
			return nullptr;
		}
		return Value->AsObject();
	}

	bool ParseEnvelope(const FString& Json, FBridgeEnvelope& Out)
	{
		TSharedPtr<FJsonObject> Root;
		if (!ParseJsonObject(Json, Root))
		{
			return false;
		}

		double CodeNumber = -1.0;
		if (Root->TryGetNumberField(TEXT("code"), CodeNumber))
		{
			Out.Code = static_cast<int32>(CodeNumber);
		}
		Root->TryGetStringField(TEXT("message"), Out.Message);
		Root->TryGetStringField(TEXT("content"), Out.Content);
		return true;
	}

	int32 GetIntField(const TSharedPtr<FJsonObject>& Root, const TCHAR* Field, int32 DefaultValue = 0)
	{
		if (!Root.IsValid())
		{
			return DefaultValue;
		}
		double Number = static_cast<double>(DefaultValue);
		if (Root->TryGetNumberField(Field, Number))
		{
			return static_cast<int32>(Number);
		}
		return DefaultValue;
	}

	void ParseErrorInner(const FString& InnerJson, int32& OutCode, FString& OutMessage)
	{
		OutCode = -1;
		OutMessage.Reset();

		TSharedPtr<FJsonObject> Root;
		if (!ParseJsonObject(InnerJson, Root))
		{
			OutMessage = InnerJson;
			return;
		}

		double CodeNumber = -1.0;
		if (Root->TryGetNumberField(TEXT("code"), CodeNumber))
		{
			OutCode = static_cast<int32>(CodeNumber);
		}
		Root->TryGetStringField(TEXT("msg"), OutMessage);
		if (OutMessage.IsEmpty())
		{
			Root->TryGetStringField(TEXT("errorMessage"), OutMessage);
		}
		if (OutMessage.IsEmpty())
		{
			OutMessage = TEXT("unknown");
		}
	}

	void FireError(const FTapOnBattleError& OnError, int32 Code, FString Message)
	{
		RunOnGameThread([OnError, Code, Message = MoveTemp(Message)]
		{
			OnError.ExecuteIfBound(Code, Message);
		});
	}

	bool HandleEnvelopeOrFireError(
		const FString& Tag,
		bool bHasResult,
		const FString& RawJson,
		const FTapOnBattleError& OnError,
		FBridgeEnvelope& OutEnvelope,
		const FTapOpenlogTracker* Tracker = nullptr,
		const FString& SessionId = FString())
	{
		if (!bHasResult)
		{
			const FString Msg = FString::Printf(TEXT("%s: native bridge returned nil"), *Tag);
			if (Tracker) { Tracker->TrackFailure(Tag, SessionId, TEXT("-1"), Msg); }
			FireError(OnError, -1, Msg);
			return false;
		}
		if (!ParseEnvelope(RawJson, OutEnvelope))
		{
			const FString Msg = FString::Printf(TEXT("%s: envelope parse failed: %s"), *Tag, *RawJson);
			if (Tracker) { Tracker->TrackFailure(Tag, SessionId, TEXT("-1"), Msg); }
			FireError(OnError, -1, Msg);
			return false;
		}
		if (OutEnvelope.Code != 0)
		{
			int32 InnerCode = -1;
			FString InnerMessage;
			ParseErrorInner(OutEnvelope.Content, InnerCode, InnerMessage);
			const FString Msg = FString::Printf(TEXT("%s failed: %s"), *Tag, *InnerMessage);
			if (Tracker) { Tracker->TrackFailure(Tag, SessionId, FString::FromInt(InnerCode), Msg); }
			FireError(OnError, InnerCode, Msg);
			return false;
		}
		return true;
	}

	bool ParsePlayerInfo(const TSharedPtr<FJsonObject>& Root, FTapBattlePlayerInfo& Out)
	{
		if (!Root.IsValid())
		{
			return false;
		}
		Root->TryGetStringField(TEXT("id"), Out.Id);
		Out.Status = GetIntField(Root, TEXT("status"));
		Out.CustomStatus = GetIntField(Root, TEXT("customStatus"));
		Root->TryGetStringField(TEXT("customProperties"), Out.CustomProperties);
		return true;
	}

	bool ParseRoomInfo(const TSharedPtr<FJsonObject>& Root, FTapBattleRoomInfo& Out)
	{
		if (!Root.IsValid())
		{
			return false;
		}

		Root->TryGetStringField(TEXT("id"), Out.Id);
		Root->TryGetStringField(TEXT("name"), Out.Name);
		Root->TryGetStringField(TEXT("type"), Out.Type);
		Root->TryGetStringField(TEXT("ownerId"), Out.OwnerId);
		Out.Status = GetIntField(Root, TEXT("status"), -1);
		Root->TryGetStringField(TEXT("customProperties"), Out.CustomProperties);
		Out.MaxPlayerCount = GetIntField(Root, TEXT("maxPlayerCount"));
		Root->TryGetStringField(TEXT("createTime"), Out.CreateTime);

		const TArray<TSharedPtr<FJsonValue>>* Players = nullptr;
		if (Root->TryGetArrayField(TEXT("players"), Players))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Players)
			{
				FTapBattlePlayerInfo Player;
				if (ParsePlayerInfo(Value.IsValid() ? Value->AsObject() : nullptr, Player))
				{
					Out.Players.Add(MoveTemp(Player));
				}
			}
		}
		return true;
	}

	TSharedPtr<FJsonObject> UnwrapRoomInfoObject(const TSharedPtr<FJsonObject>& Root)
	{
		const TSharedPtr<FJsonObject> RoomInfo = GetObjectField(Root, TEXT("roomInfo"));
		return RoomInfo.IsValid() ? RoomInfo : Root;
	}

	bool ParseRoomInfoJson(const FString& Json, FTapBattleRoomInfo& Out)
	{
		TSharedPtr<FJsonObject> Root;
		return ParseJsonObject(Json, Root) && ParseRoomInfo(UnwrapRoomInfoObject(Root), Out);
	}

	bool ParseRoomBasicInfo(const TSharedPtr<FJsonObject>& Root, FTapBattleRoomBasicInfo& Out)
	{
		if (!Root.IsValid())
		{
			return false;
		}
		Root->TryGetStringField(TEXT("id"), Out.Id);
		Root->TryGetStringField(TEXT("name"), Out.Name);
		Root->TryGetStringField(TEXT("type"), Out.Type);
		Root->TryGetStringField(TEXT("customProperties"), Out.CustomProperties);
		Out.MaxPlayerCount = GetIntField(Root, TEXT("maxPlayerCount"));
		Out.PlayerCount = GetIntField(Root, TEXT("playerCount"));
		Root->TryGetStringField(TEXT("createTime"), Out.CreateTime);
		return true;
	}

	bool ParseRoomListJson(const FString& Json, FTapBattleRoomListData& Out)
	{
		TSharedPtr<FJsonObject> Root;
		if (!ParseJsonObject(Json, Root))
		{
			return false;
		}

		Out.Offset = GetIntField(Root, TEXT("offset"));
		Root->TryGetBoolField(TEXT("hasMore"), Out.bHasMore);

		const TArray<TSharedPtr<FJsonValue>>* Rooms = nullptr;
		if (Root->TryGetArrayField(TEXT("rooms"), Rooms))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Rooms)
			{
				FTapBattleRoomBasicInfo Room;
				if (ParseRoomBasicInfo(Value.IsValid() ? Value->AsObject() : nullptr, Room))
				{
					Out.Rooms.Add(MoveTemp(Room));
				}
			}
		}
		return true;
	}

	bool ParseFrameData(const TSharedPtr<FJsonObject>& Root, FTapBattleFrameData& Out)
	{
		if (!Root.IsValid())
		{
			return false;
		}
		Out.Id = GetIntField(Root, TEXT("id"));

		const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
		if (Root->TryGetArrayField(TEXT("inputs"), Inputs))
		{
			for (const TSharedPtr<FJsonValue>& Value : *Inputs)
			{
				const TSharedPtr<FJsonObject> InputObj = Value.IsValid() ? Value->AsObject() : nullptr;
				if (!InputObj.IsValid())
				{
					continue;
				}
				FTapBattlePlayerInputInfo Input;
				InputObj->TryGetStringField(TEXT("playerId"), Input.PlayerId);
				InputObj->TryGetStringField(TEXT("data"), Input.Data);
				InputObj->TryGetStringField(TEXT("serverTms"), Input.ServerTime);
				Out.Inputs.Add(MoveTemp(Input));
			}
		}
		return true;
	}

	bool ParseFrameSyncInfo(const TSharedPtr<FJsonObject>& Root, FTapBattleFrameSyncInfo& Out)
	{
		if (!Root.IsValid())
		{
			return false;
		}
		ParseRoomInfo(GetObjectField(Root, TEXT("roomInfo")), Out.RoomInfo);
		Out.FrameSyncId = GetIntField(Root, TEXT("frameSyncId"));
		Out.Seed = GetIntField(Root, TEXT("seed"));
		Root->TryGetStringField(TEXT("serverTms"), Out.ServerTime);
		Root->TryGetStringField(TEXT("serverTime"), Out.ServerTime);
		return true;
	}

	TSharedRef<FJsonObject> SerializeRoomConfig(const FTapBattleRoomConfig& Config)
	{
		TSharedRef<FJsonObject> Room = MakeShared<FJsonObject>();
		Room->SetNumberField(TEXT("maxPlayerCount"), Config.MaxPlayerCount);
		Room->SetStringField(TEXT("type"), Config.Type);
		Room->SetStringField(TEXT("name"), Config.Name);
		Room->SetStringField(TEXT("customProperties"), Config.CustomProperties);

		TSharedRef<FJsonObject> MatchParams = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& Pair : Config.MatchParams)
		{
			MatchParams->SetStringField(Pair.Key, Pair.Value);
		}
		Room->SetObjectField(TEXT("matchParams"), MatchParams);
		return Room;
	}

	TSharedRef<FJsonObject> SerializePlayerConfig(const FTapBattlePlayerConfig& Config)
	{
		TSharedRef<FJsonObject> Player = MakeShared<FJsonObject>();
		Player->SetNumberField(TEXT("customStatus"), Config.CustomStatus);
		Player->SetStringField(TEXT("customProperties"), Config.CustomProperties);
		return Player;
	}

	FString SerializeCreateRoomConfig(const FTapBattleCreateRoomConfig& Config)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetObjectField(TEXT("roomCfg"), SerializeRoomConfig(Config.RoomConfig));
		Root->SetObjectField(TEXT("playerCfg"), SerializePlayerConfig(Config.PlayerConfig));
		return JsonObjectToString(Root);
	}

	FString SerializeMatchRoomConfig(const FTapBattleMatchRoomConfig& Config)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetObjectField(TEXT("roomCfg"), SerializeRoomConfig(Config.RoomConfig));
		Root->SetObjectField(TEXT("playerCfg"), SerializePlayerConfig(Config.PlayerConfig));
		return JsonObjectToString(Root);
	}

	FString SerializeGetRoomListRequest(const FString& RoomType, int32 Offset, int32 Limit)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("roomType"), RoomType);
		Root->SetNumberField(TEXT("offset"), Offset);
		Root->SetNumberField(TEXT("limit"), Limit);
		return JsonObjectToString(Root);
	}

	FString SerializeJoinRoomRequest(const FString& RoomId, const FTapBattlePlayerConfig& PlayerConfig)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("roomId"), RoomId);
		Root->SetObjectField(TEXT("playerCfg"), SerializePlayerConfig(PlayerConfig));
		return JsonObjectToString(Root);
	}

	FString SerializeUpdateRoomProperties(const FString& RoomName, const FString& Properties)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("name"), RoomName);
		Root->SetStringField(TEXT("customProperties"), Properties);
		return JsonObjectToString(Root);
	}

	FString SerializeStringField(const TCHAR* Field, const FString& Value)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(Field, Value);
		return JsonObjectToString(Root);
	}

	FString SerializeCustomMessage(const FString& Msg, int32 Type, const TArray<FString>& Receivers)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("msg"), Msg);
		Root->SetNumberField(TEXT("type"), Type);

		TArray<TSharedPtr<FJsonValue>> ReceiverArray;
		for (const FString& Receiver : Receivers)
		{
			ReceiverArray.Add(MakeShared<FJsonValueString>(Receiver));
		}
		Root->SetArrayField(TEXT("receivers"), ReceiverArray);
		return JsonObjectToString(Root);
	}

	int32 ComputeRegion(const FTapTapSdkOptions& Config)
	{
		const bool bGlobal = Config.Region == ETapRegion::Global;
		if (FTapSdkConfigState::IsRNDEnvironment())
		{
			return bGlobal ? 3 : 2;
		}
		return bGlobal ? 1 : 0;
	}

	FString LanguageToString(ETapLanguage Language)
	{
		switch (Language)
		{
		case ETapLanguage::zh_Hans: return TEXT("zh-CN");
		case ETapLanguage::zh_Hant: return TEXT("zh-TW");
		case ETapLanguage::en: return TEXT("en");
		case ETapLanguage::ja: return TEXT("ja");
		case ETapLanguage::ko: return TEXT("ko");
		case ETapLanguage::th: return TEXT("th");
		case ETapLanguage::id: return TEXT("id");
		case ETapLanguage::de: return TEXT("de");
		case ETapLanguage::es: return TEXT("es");
		case ETapLanguage::fr: return TEXT("fr");
		case ETapLanguage::pt: return TEXT("pt");
		case ETapLanguage::ru: return TEXT("ru");
		case ETapLanguage::tr: return TEXT("tr");
		case ETapLanguage::vi: return TEXT("vi");
		case ETapLanguage::Auto:
		default:
			return TEXT("zh-CN");
		}
	}

	bool CheckPCLaunchState(FString& OutError);
	bool CheckReadyForNativeCall(const FString& Api, const FTapOnBattleError& OnError,
		const FTapOpenlogTracker* Tracker = nullptr, const FString& SessionId = FString());

	FString MakeInitConfigJson()
	{
		const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("region"), ComputeRegion(Config));
		Root->SetNumberField(TEXT("logToConsole"), Config.bEnableLog ? 1 : 0);
		Root->SetNumberField(TEXT("logLevel"), Config.bEnableLog ? 1 : 3);
		Root->SetStringField(TEXT("dataDir"), FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("onlinebattle_") + Config.ClientID));
		Root->SetStringField(TEXT("ua"), FString::Printf(TEXT("TapSDK-UE5/%s"), *FEngineVersion::Current().ToString()));
		Root->SetStringField(TEXT("lang"), LanguageToString(Config.PreferredLanguage));
		Root->SetStringField(TEXT("platform"), TEXT("PC"));
		Root->SetStringField(TEXT("deviceId"), FPlatformMisc::GetLoginId());
		Root->SetStringField(TEXT("sdkArtifact"), TEXT("UE5"));
		const FString NativeVersion = TapBattleDesktopBridge::GetVersion();
		Root->SetStringField(TEXT("sdkModuleVer"), NativeVersion.IsEmpty() ? FString(TEXT("4.10.0")) : NativeVersion);
		return JsonObjectToString(Root);
	}

	bool MakeConnectConfigJson(FString& OutJson, FString& OutError, int32& OutCode)
	{
		OutCode = TapBattleResultCode::NEED_LOGIN;
		if (!TapBattleDesktopBridge::IsReady())
		{
			OutError = TEXT("native bridge is not initialized");
			OutCode = TapBattleResultCode::INIT_FAIL;
			return false;
		}

		FString LaunchError;
		if (!CheckPCLaunchState(LaunchError))
		{
			OutError = LaunchError;
			OutCode = TapBattleResultCode::ERROR_NOT_LAUNCHED_BY_TAP_CLIENT;
			return false;
		}

		const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
		if (Config.ClientID.IsEmpty() || Config.ClientToken.IsEmpty())
		{
			OutError = TEXT("TapCore clientId/clientToken is empty");
			return false;
		}

		TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
		if (!Account.IsSet())
		{
			OutError = TEXT("TapLogin account is empty");
			return false;
		}
		if (Account->AccessToken.Kid.IsEmpty() || Account->AccessToken.MacKey.IsEmpty())
		{
			OutError = TEXT("TapLogin account is missing kid/macKey");
			return false;
		}

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("clientId"), Config.ClientID);
		Root->SetStringField(TEXT("clientToken"), Config.ClientToken);

		TSharedRef<FJsonObject> Token = MakeShared<FJsonObject>();
		Token->SetStringField(TEXT("kid"), Account->AccessToken.Kid);
		Token->SetStringField(TEXT("key"), Account->AccessToken.MacKey);
		Root->SetObjectField(TEXT("accessToken"), Token);

		OutJson = JsonObjectToString(Root);
		return true;
	}

	bool CheckPCLaunchState(FString& OutError)
	{
#if PLATFORM_WINDOWS
		if (!UTapLauncher::IsPassedLauncherCheck())
		{
			OutError = TEXT("IsLaunchedFromTapTapPC validation failed");
			return false;
		}
#endif
		return true;
	}

	bool CheckReadyForNativeCall(const FString& Api, const FTapOnBattleError& OnError,
		const FTapOpenlogTracker* Tracker, const FString& SessionId)
	{
		if (!TapBattleDesktopBridge::IsReady())
		{
			const FString Msg = FString::Printf(TEXT("%s failed: native bridge is not initialized"), *Api);
			if (Tracker) { Tracker->TrackFailure(Api, SessionId, FString::FromInt(TapBattleResultCode::INIT_FAIL), Msg); }
			FireError(OnError, TapBattleResultCode::INIT_FAIL, Msg);
			return false;
		}
		FString LaunchError;
		if (!CheckPCLaunchState(LaunchError))
		{
			if (Tracker) { Tracker->TrackFailure(Api, SessionId, FString::FromInt(TapBattleResultCode::ERROR_NOT_LAUNCHED_BY_TAP_CLIENT), LaunchError); }
			FireError(OnError, TapBattleResultCode::ERROR_NOT_LAUNCHED_BY_TAP_CLIENT, LaunchError);
			return false;
		}
		return true;
	}

	FString GetClientId()
	{
		return FTapSdkConfigState::GetCurrentConfig().ClientID;
	}
}

FTapBattlePlatformDesktop::FTapBattlePlatformDesktop() = default;

FTapBattlePlatformDesktop::~FTapBattlePlatformDesktop()
{
	TapBattleDesktopBridge::Finalize();
}

void FTapBattlePlatformDesktop::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	Tracker = FTapOpenlogTracker(TEXT("TapOnlineBattle"), UTapBattle::GetVersion());
	Tracker.TrackInit();

	TWeakPtr<FTapBattlePlatformDesktop> WeakSelf = AsShared();
	bInitialized = TapBattleDesktopBridge::Init(MakeInitConfigJson(), [WeakSelf](const FString& RawJson)
	{
		if (TSharedPtr<FTapBattlePlatformDesktop> Self = WeakSelf.Pin())
		{
			Self->HandleNotification(RawJson);
		}
	});

	if (!bInitialized)
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Desktop] Initialize failed"));
		return;
	}

	UE_LOG(LogTapBattle, Log, TEXT("[TapBattle][Desktop] Initialize complete"));
}

bool FTapBattlePlatformDesktop::IsInitialized() const
{
	return bInitialized;
}

void FTapBattlePlatformDesktop::HandleNotification(const FString& RawJson)
{
	FBridgeEnvelope Envelope;
	if (!ParseEnvelope(RawJson, Envelope) || Envelope.Code != 0)
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Desktop] notification envelope parse failed: %s"), *RawJson);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	if (!ParseJsonObject(Envelope.Content, Root))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Desktop] notification content parse failed: %s"), *Envelope.Content);
		return;
	}

	FString Kind;
	Root->TryGetStringField(TEXT("kind"), Kind);
	if (Kind == TEXT("state"))
	{
		FString StateName;
		Root->TryGetStringField(TEXT("state"), StateName);
		const int32 RawCode = GetIntField(Root, TEXT("rawCode"));

		FString Message;
		if (TSharedPtr<FJsonObject> ErrorObj = GetObjectField(Root, TEXT("error")))
		{
			ErrorObj->TryGetStringField(TEXT("msg"), Message);
			if (Message.IsEmpty())
			{
				ErrorObj->TryGetStringField(TEXT("errorMessage"), Message);
			}
		}

		if (StateName == TEXT("disconnected"))
		{
			FTapOnlineBattleCallbackManager::Get().DispatchDisconnected(RawCode, Message);
		}
		else if (StateName == TEXT("battleServiceError"))
		{
			FTapOnlineBattleCallbackManager::Get().DispatchBattleServiceError();
		}
		else
		{
			UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle] unknown state notification: %s"), *StateName);
		}
		return;
	}

	if (Kind != TEXT("event"))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Desktop] unknown notification kind: %s"), *Kind);
		return;
	}

	FString EventName;
	Root->TryGetStringField(TEXT("event"), EventName);
	const TSharedPtr<FJsonObject> Data = GetObjectField(Root, TEXT("data"));
	if (!Data.IsValid())
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Desktop] notification data missing for event: %s"), *EventName);
		return;
	}

	if (EventName == TEXT("roomPropertiesChanged"))
	{
		FString RoomId, Name, CustomProperties;
		Data->TryGetStringField(TEXT("id"), RoomId);
		Data->TryGetStringField(TEXT("name"), Name);
		Data->TryGetStringField(TEXT("customProperties"), CustomProperties);
		FTapOnlineBattleCallbackManager::Get().DispatchRoomPropertiesChanged(RoomId, Name, CustomProperties);
	}
	else if (EventName == TEXT("playerCustomPropertiesChanged"))
	{
		FString PlayerId, Properties;
		Data->TryGetStringField(TEXT("playerId"), PlayerId);
		Data->TryGetStringField(TEXT("properties"), Properties);
		FTapOnlineBattleCallbackManager::Get().DispatchPlayerCustomPropertiesChanged(PlayerId, Properties);
	}
	else if (EventName == TEXT("playerCustomStatusChanged"))
	{
		FString PlayerId;
		Data->TryGetStringField(TEXT("playerId"), PlayerId);
		FTapOnlineBattleCallbackManager::Get().DispatchPlayerCustomStatusChanged(PlayerId, GetIntField(Data, TEXT("status")));
	}
	else if (EventName == TEXT("frameSyncStarted"))
	{
		FTapBattleFrameSyncInfo FrameSyncInfo;
		ParseFrameSyncInfo(Data, FrameSyncInfo);
		FTapOnlineBattleCallbackManager::Get().DispatchFrameSyncStarted(FrameSyncInfo);
	}
	else if (EventName == TEXT("frameReceived"))
	{
		FTapBattleFrameData FrameData;
		ParseFrameData(Data, FrameData);
		FTapOnlineBattleCallbackManager::Get().DispatchFrameReceived(FrameData);
	}
	else if (EventName == TEXT("frameSyncStopped"))
	{
		FString RoomId;
		Data->TryGetStringField(TEXT("roomId"), RoomId);
		FTapOnlineBattleCallbackManager::Get().DispatchFrameSyncStopped(RoomId, GetIntField(Data, TEXT("frameSyncId")), GetIntField(Data, TEXT("reason")));
	}
	else if (EventName == TEXT("playerOffline") || EventName == TEXT("playerLeft"))
	{
		FString RoomId, RoomOwnerId, PlayerId;
		Data->TryGetStringField(TEXT("roomId"), RoomId);
		Data->TryGetStringField(TEXT("roomOwnerId"), RoomOwnerId);
		Data->TryGetStringField(TEXT("playerId"), PlayerId);
		if (EventName == TEXT("playerOffline"))
		{
			FTapOnlineBattleCallbackManager::Get().DispatchPlayerOffline(RoomId, RoomOwnerId, PlayerId);
		}
		else
		{
			FTapOnlineBattleCallbackManager::Get().DispatchPlayerLeft(RoomId, RoomOwnerId, PlayerId);
		}
	}
	else if (EventName == TEXT("playerEntered"))
	{
		FString RoomId;
		FTapBattlePlayerInfo PlayerInfo;
		Data->TryGetStringField(TEXT("roomId"), RoomId);
		ParsePlayerInfo(GetObjectField(Data, TEXT("playerInfo")), PlayerInfo);
		FTapOnlineBattleCallbackManager::Get().DispatchPlayerEntered(RoomId, PlayerInfo);
	}
	else if (EventName == TEXT("customMessageReceived"))
	{
		FString PlayerId, Message;
		Data->TryGetStringField(TEXT("playerId"), PlayerId);
		Data->TryGetStringField(TEXT("msg"), Message);
		FTapOnlineBattleCallbackManager::Get().DispatchCustomMessageReceived(PlayerId, Message);
	}
	else if (EventName == TEXT("playerKicked"))
	{
		FString RoomId, PlayerId;
		Data->TryGetStringField(TEXT("roomId"), RoomId);
		Data->TryGetStringField(TEXT("playerId"), PlayerId);
		FTapOnlineBattleCallbackManager::Get().DispatchPlayerKicked(RoomId, PlayerId);
	}
	else
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle] unknown event notification: %s"), *EventName);
	}
}

void FTapBattlePlatformDesktop::SetLogLevel(int32 Level, bool bLogToConsole)
{
	TapBattleDesktopBridge::SetLogLevel(Level, bLogToConsole);
}

FString FTapBattlePlatformDesktop::GetNativeVersion()
{
	return TapBattleDesktopBridge::GetVersion();
}

int64 FTapBattlePlatformDesktop::NewRandomNumberGenerator(int32 Seed)
{
	return TapBattleDesktopBridge::NewRandomNumberGenerator(Seed);
}

int32 FTapBattlePlatformDesktop::RandomInt(int64 ObjId)
{
	return TapBattleDesktopBridge::RandomInt(ObjId);
}

void FTapBattlePlatformDesktop::FreeRandomNumberGenerator(int64 ObjId)
{
	TapBattleDesktopBridge::FreeRandomNumberGenerator(ObjId);
}

void FTapBattlePlatformDesktop::Connect(const FTapOnBattlePlayerIdResult& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("connect"), SessionId);

	FString ConfigJson;
	FString Error;
	int32 ErrorCode = TapBattleResultCode::NEED_LOGIN;
	if (!MakeConnectConfigJson(ConfigJson, Error, ErrorCode))
	{
		Tracker.TrackFailure(TEXT("connect"), SessionId, FString::FromInt(ErrorCode), Error);
		FireError(OnError, ErrorCode, Error);
		return;
	}

	TWeakPtr<FTapBattlePlatformDesktop> WeakSelf = AsShared();
	TapBattleDesktopBridge::Connect(ConfigJson, [WeakSelf, OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		if (!WeakSelf.IsValid()) { return; }

		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("connect"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }

		TSharedPtr<FJsonObject> Root;
		if (!ParseJsonObject(Envelope.Content, Root))
		{
			const FString Msg = FString::Printf(TEXT("connect: content parse failed: %s"), *Envelope.Content);
			Tracker.TrackFailure(TEXT("connect"), SessionId, TEXT("-1"), Msg);
			FireError(OnError, -1, Msg);
			return;
		}
		FString PlayerId;
		Root->TryGetStringField(TEXT("playerId"), PlayerId);
		Tracker.TrackSuccess(TEXT("connect"), SessionId);
		RunOnGameThread([OnSuccess, PlayerId]
		{
			OnSuccess.ExecuteIfBound(PlayerId);
		});
	});
}

void FTapBattlePlatformDesktop::Disconnect(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("disconnect"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("disconnect"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::Disconnect(GetClientId(), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("disconnect"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("disconnect"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::CreateRoom(const FTapBattleCreateRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("createRoom"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("createRoom"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::CreateRoom(GetClientId(), SerializeCreateRoomConfig(Config), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("createRoom"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }

		FTapBattleRoomInfo Room;
		if (!ParseRoomInfoJson(Envelope.Content, Room))
		{
			const FString Msg = FString::Printf(TEXT("createRoom: room parse failed: %s"), *Envelope.Content);
			Tracker.TrackFailure(TEXT("createRoom"), SessionId, TEXT("-1"), Msg);
			FireError(OnError, -1, Msg);
			return;
		}
		Tracker.TrackSuccess(TEXT("createRoom"), SessionId);
		RunOnGameThread([OnSuccess, Room] { OnSuccess.ExecuteIfBound(Room); });
	});
}

void FTapBattlePlatformDesktop::MatchRoom(const FTapBattleMatchRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("matchRoom"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("matchRoom"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::MatchRoom(GetClientId(), SerializeMatchRoomConfig(Config), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("matchRoom"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }

		FTapBattleRoomInfo Room;
		if (!ParseRoomInfoJson(Envelope.Content, Room))
		{
			const FString Msg = FString::Printf(TEXT("matchRoom: room parse failed: %s"), *Envelope.Content);
			Tracker.TrackFailure(TEXT("matchRoom"), SessionId, TEXT("-1"), Msg);
			FireError(OnError, -1, Msg);
			return;
		}
		Tracker.TrackSuccess(TEXT("matchRoom"), SessionId);
		RunOnGameThread([OnSuccess, Room] { OnSuccess.ExecuteIfBound(Room); });
	});
}

void FTapBattlePlatformDesktop::GetRoomList(const FString& RoomType, int32 Offset, int32 Limit, const FTapOnBattleRoomListResult& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("getRoomList"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("getRoomList"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::GetRoomList(GetClientId(), SerializeGetRoomListRequest(RoomType, Offset, Limit), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("getRoomList"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }

		FTapBattleRoomListData Rooms;
		if (!ParseRoomListJson(Envelope.Content, Rooms))
		{
			const FString Msg = FString::Printf(TEXT("getRoomList: list parse failed: %s"), *Envelope.Content);
			Tracker.TrackFailure(TEXT("getRoomList"), SessionId, TEXT("-1"), Msg);
			FireError(OnError, -1, Msg);
			return;
		}
		Tracker.TrackSuccess(TEXT("getRoomList"), SessionId);
		RunOnGameThread([OnSuccess, Rooms] { OnSuccess.ExecuteIfBound(Rooms); });
	});
}

void FTapBattlePlatformDesktop::JoinRoom(const FString& RoomId, const FTapBattlePlayerConfig& PlayerConfig, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("joinRoom"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("joinRoom"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::JoinRoom(GetClientId(), SerializeJoinRoomRequest(RoomId, PlayerConfig), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("joinRoom"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }

		FTapBattleRoomInfo Room;
		if (!ParseRoomInfoJson(Envelope.Content, Room))
		{
			const FString Msg = FString::Printf(TEXT("joinRoom: room parse failed: %s"), *Envelope.Content);
			Tracker.TrackFailure(TEXT("joinRoom"), SessionId, TEXT("-1"), Msg);
			FireError(OnError, -1, Msg);
			return;
		}
		Tracker.TrackSuccess(TEXT("joinRoom"), SessionId);
		RunOnGameThread([OnSuccess, Room] { OnSuccess.ExecuteIfBound(Room); });
	});
}

void FTapBattlePlatformDesktop::LeaveRoom(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("leaveRoom"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("leaveRoom"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::LeaveRoom(GetClientId(), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("leaveRoom"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("leaveRoom"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::UpdatePlayerCustomStatus(int32 Status, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("updatePlayerCustomStatus"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("updatePlayerCustomStatus"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::UpdatePlayerCustomStatus(GetClientId(), Status, [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updatePlayerCustomStatus"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("updatePlayerCustomStatus"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::UpdatePlayerCustomProperties(const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("updatePlayerCustomProperties"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("updatePlayerCustomProperties"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::UpdatePlayerCustomProperties(GetClientId(), Properties, [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updatePlayerCustomProperties"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("updatePlayerCustomProperties"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::UpdateRoomProperties(const FString& RoomName, const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("updateRoomProperties"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("updateRoomProperties"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::UpdateRoomProperties(GetClientId(), SerializeUpdateRoomProperties(RoomName, Properties), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updateRoomProperties"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("updateRoomProperties"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::KickRoomPlayer(const FString& PlayerId, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("kickRoomPlayer"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("kickRoomPlayer"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::KickRoomPlayer(GetClientId(), PlayerId, [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("kickRoomPlayer"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("kickRoomPlayer"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::StartFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("startFrameSync"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("startFrameSync"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::StartFrameSync(GetClientId(), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("startFrameSync"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("startFrameSync"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::SendFrameInput(const FString& Data, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("sendFrameInput"), OnError)) { return; }

	TapBattleDesktopBridge::SendFrameInput(GetClientId(), SerializeStringField(TEXT("data"), Data), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("sendFrameInput"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::StopFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("stopFrameSync"), SessionId);

	if (!CheckReadyForNativeCall(TEXT("stopFrameSync"), OnError, &Tracker, SessionId)) { return; }

	TapBattleDesktopBridge::StopFrameSync(GetClientId(), [OnSuccess, OnError, Tracker = Tracker, SessionId](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("stopFrameSync"), bHasResult, Raw, OnError, Envelope, &Tracker, SessionId)) { return; }
		Tracker.TrackSuccess(TEXT("stopFrameSync"), SessionId);
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformDesktop::SendCustomMessage(const FString& Msg, int32 Type, const TArray<FString>& Receivers, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("sendCustomMessage"), OnError)) { return; }

	TapBattleDesktopBridge::SendCustomMessage(GetClientId(), SerializeCustomMessage(Msg, Type, Receivers), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("sendCustomMessage"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

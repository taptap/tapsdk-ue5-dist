// Copyright TapTap. All Rights Reserved.

#include "Platform/TapBattlePlatformAndroid.h"

#if PLATFORM_ANDROID

#include "Mobile/Android/TapBattleAndroidBridge.h"
#include "TapBattleModule.h"
#include "TapOnlineBattleCallbackManager.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "TapJsonCompat.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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

	FString SerializeObject(const TSharedRef<FJsonObject>& Obj)
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
		if (Root->TryGetNumberField(TEXT("errorCode"), CodeNumber))
		{
			OutCode = static_cast<int32>(CodeNumber);
		}
		Root->TryGetStringField(TEXT("errorMessage"), OutMessage);
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
		FBridgeEnvelope& OutEnvelope)
	{
		if (!bHasResult)
		{
			FireError(OnError, -1, FString::Printf(TEXT("%s: native bridge returned nil"), *Tag));
			return false;
		}
		if (!ParseEnvelope(RawJson, OutEnvelope))
		{
			FireError(OnError, -1, FString::Printf(TEXT("%s: envelope parse failed: %s"), *Tag, *RawJson));
			return false;
		}
		if (OutEnvelope.Code != 0)
		{
			int32 InnerCode = -1;
			FString InnerMessage;
			ParseErrorInner(OutEnvelope.Content, InnerCode, InnerMessage);
			FireError(OnError, InnerCode, FString::Printf(TEXT("%s failed: %s"), *Tag, *InnerMessage));
			return false;
		}
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

	bool ParseRoomInfoJson(const FString& Json, FTapBattleRoomInfo& Out)
	{
		TSharedPtr<FJsonObject> Root;
		return ParseJsonObject(Json, Root) && ParseRoomInfo(Root, Out);
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
		return SerializeObject(Root);
	}

	FString SerializeMatchRoomConfig(const FTapBattleMatchRoomConfig& Config)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetObjectField(TEXT("roomCfg"), SerializeRoomConfig(Config.RoomConfig));
		Root->SetObjectField(TEXT("playerCfg"), SerializePlayerConfig(Config.PlayerConfig));
		return SerializeObject(Root);
	}

	FString SerializeGetRoomListRequest(const FString& RoomType, int32 Offset, int32 Limit)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("roomType"), RoomType);
		Root->SetNumberField(TEXT("offset"), Offset);
		Root->SetNumberField(TEXT("limit"), Limit);
		return SerializeObject(Root);
	}

	FString SerializeJoinRoomRequest(const FString& RoomId, const FTapBattlePlayerConfig& PlayerConfig)
	{
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(TEXT("roomId"), RoomId);
		Root->SetObjectField(TEXT("playerCfg"), SerializePlayerConfig(PlayerConfig));
		return SerializeObject(Root);
	}
}

FTapBattlePlatformAndroid::FTapBattlePlatformAndroid() = default;

FTapBattlePlatformAndroid::~FTapBattlePlatformAndroid()
{
	TapBattleAndroidBridge::UnregisterNotificationCallback();
}

void FTapBattlePlatformAndroid::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	TapBattleAndroidBridge::Init();

	TWeakPtr<FTapBattlePlatformAndroid> WeakSelf = AsShared();
	bInitialized = TapBattleAndroidBridge::RegisterNotificationCallback([WeakSelf](const FString& RawJson)
	{
		if (TSharedPtr<FTapBattlePlatformAndroid> Self = WeakSelf.Pin())
		{
			Self->HandleNotification(RawJson);
		}
	});

	if (!bInitialized)
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] Initialize failed"));
		return;
	}

	UE_LOG(LogTapBattle, Log, TEXT("[TapBattle][Android] Initialize complete"));
}

bool FTapBattlePlatformAndroid::IsInitialized() const
{
	return bInitialized;
}

void FTapBattlePlatformAndroid::HandleNotification(const FString& RawJson)
{
	FBridgeEnvelope Envelope;
	if (!ParseEnvelope(RawJson, Envelope) || Envelope.Code != 0)
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] notification envelope parse failed: %s"), *RawJson);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	if (!ParseJsonObject(Envelope.Content, Root))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] notification content parse failed: %s"), *Envelope.Content);
		return;
	}

	FString Kind;
	Root->TryGetStringField(TEXT("kind"), Kind);
	if (Kind == TEXT("state"))
	{
		FString StateName;
		Root->TryGetStringField(TEXT("state"), StateName);
		const int32 RawCode = GetIntField(Root, TEXT("rawCode"));

		TSharedPtr<FJsonObject> ErrorObj = GetObjectField(Root, TEXT("error"));
		FString Message;
		if (ErrorObj.IsValid())
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
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] unknown notification kind: %s"), *Kind);
		return;
	}

	FString EventName;
	Root->TryGetStringField(TEXT("event"), EventName);
	const TSharedPtr<FJsonObject> Data = GetObjectField(Root, TEXT("data"));
	if (!Data.IsValid())
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][Android] notification data missing for event: %s"), *EventName);
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

void FTapBattlePlatformAndroid::SetLogLevel(int32 Level, bool bLogToConsole)
{
	TapBattleAndroidBridge::SetLogLevel(Level, bLogToConsole);
}

FString FTapBattlePlatformAndroid::GetNativeVersion()
{
	return TapBattleAndroidBridge::GetVersion();
}

int64 FTapBattlePlatformAndroid::NewRandomNumberGenerator(int32 Seed)
{
	return TapBattleAndroidBridge::NewRandomNumberGenerator(Seed);
}

int32 FTapBattlePlatformAndroid::RandomInt(int64 ObjId)
{
	return TapBattleAndroidBridge::RandomInt(ObjId);
}

void FTapBattlePlatformAndroid::FreeRandomNumberGenerator(int64 ObjId)
{
	TapBattleAndroidBridge::FreeRandomNumberGenerator(ObjId);
}

void FTapBattlePlatformAndroid::Connect(const FTapOnBattlePlayerIdResult& OnSuccess, const FTapOnBattleError& OnError)
{
	TWeakPtr<FTapBattlePlatformAndroid> WeakSelf = AsShared();
	TapBattleAndroidBridge::Connect([WeakSelf, OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		if (!WeakSelf.IsValid()) { return; }

		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("connect"), bHasResult, Raw, OnError, Envelope)) { return; }

		TSharedPtr<FJsonObject> Root;
		if (!ParseJsonObject(Envelope.Content, Root))
		{
			FireError(OnError, -1, FString::Printf(TEXT("connect: content parse failed: %s"), *Envelope.Content));
			return;
		}
		FString PlayerId;
		Root->TryGetStringField(TEXT("playerId"), PlayerId);
		RunOnGameThread([OnSuccess, PlayerId]
		{
			OnSuccess.ExecuteIfBound(PlayerId);
		});
	});
}

void FTapBattlePlatformAndroid::Disconnect(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::Disconnect([OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("disconnect"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::CreateRoom(const FTapBattleCreateRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::CreateRoom(SerializeCreateRoomConfig(Config), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("createRoom"), bHasResult, Raw, OnError, Envelope)) { return; }

		FTapBattleRoomInfo Room;
		if (!ParseRoomInfoJson(Envelope.Content, Room))
		{
			FireError(OnError, -1, FString::Printf(TEXT("createRoom: room parse failed: %s"), *Envelope.Content));
			return;
		}
		RunOnGameThread([OnSuccess, Room] { OnSuccess.ExecuteIfBound(Room); });
	});
}

void FTapBattlePlatformAndroid::MatchRoom(const FTapBattleMatchRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::MatchRoom(SerializeMatchRoomConfig(Config), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("matchRoom"), bHasResult, Raw, OnError, Envelope)) { return; }

		FTapBattleRoomInfo Room;
		if (!ParseRoomInfoJson(Envelope.Content, Room))
		{
			FireError(OnError, -1, FString::Printf(TEXT("matchRoom: room parse failed: %s"), *Envelope.Content));
			return;
		}
		RunOnGameThread([OnSuccess, Room] { OnSuccess.ExecuteIfBound(Room); });
	});
}

void FTapBattlePlatformAndroid::GetRoomList(const FString& RoomType, int32 Offset, int32 Limit, const FTapOnBattleRoomListResult& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::GetRoomList(SerializeGetRoomListRequest(RoomType, Offset, Limit), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("getRoomList"), bHasResult, Raw, OnError, Envelope)) { return; }

		FTapBattleRoomListData Rooms;
		if (!ParseRoomListJson(Envelope.Content, Rooms))
		{
			FireError(OnError, -1, FString::Printf(TEXT("getRoomList: list parse failed: %s"), *Envelope.Content));
			return;
		}
		RunOnGameThread([OnSuccess, Rooms] { OnSuccess.ExecuteIfBound(Rooms); });
	});
}

void FTapBattlePlatformAndroid::JoinRoom(const FString& RoomId, const FTapBattlePlayerConfig& PlayerConfig, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::JoinRoom(SerializeJoinRoomRequest(RoomId, PlayerConfig), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("joinRoom"), bHasResult, Raw, OnError, Envelope)) { return; }

		FTapBattleRoomInfo Room;
		if (!ParseRoomInfoJson(Envelope.Content, Room))
		{
			FireError(OnError, -1, FString::Printf(TEXT("joinRoom: room parse failed: %s"), *Envelope.Content));
			return;
		}
		RunOnGameThread([OnSuccess, Room] { OnSuccess.ExecuteIfBound(Room); });
	});
}

void FTapBattlePlatformAndroid::LeaveRoom(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::LeaveRoom([OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("leaveRoom"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::UpdatePlayerCustomStatus(int32 Status, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::UpdatePlayerCustomStatus(Status, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updatePlayerCustomStatus"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::UpdatePlayerCustomProperties(const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::UpdatePlayerCustomProperties(Properties, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updatePlayerCustomProperties"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::UpdateRoomProperties(const FString& RoomName, const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::UpdateRoomProperties(RoomName, Properties, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updateRoomProperties"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::KickRoomPlayer(const FString& PlayerId, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::KickRoomPlayer(PlayerId, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("kickRoomPlayer"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::StartFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::StartFrameSync([OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("startFrameSync"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::SendFrameInput(const FString& Data, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::SendFrameInput(Data, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("sendFrameInput"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::StopFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::StopFrameSync([OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("stopFrameSync"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformAndroid::SendCustomMessage(const FString& Msg, int32 Type, const TArray<FString>& Receivers, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	TapBattleAndroidBridge::SendCustomMessage(Msg, Type, Receivers, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("sendCustomMessage"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

#endif // PLATFORM_ANDROID

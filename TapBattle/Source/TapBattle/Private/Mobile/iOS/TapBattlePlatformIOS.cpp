// Copyright TapTap. All Rights Reserved.

#include "Mobile/iOS/TapBattlePlatformIOS.h"

#include "Mobile/iOS/TapBattleIOSBridge.h"
#include "TapBattleModule.h"
#include "TapOnlineBattleCallbackManager.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "TapLogin.h"
#include "Internal/Storage/TapDeviceId.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "TapJsonCompat.h"
#include "Misc/Paths.h"
#include "Misc/EngineVersion.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_IOS

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

	// 原生未初始化时直接返回清晰的 INIT_FAIL，而不是把请求发给原生再拿到难懂的 -2。
	// 对齐 Desktop 平台的 CheckReadyForNativeCall。
	bool CheckReadyForNativeCall(const TCHAR* Api, const FTapOnBattleError& OnError)
	{
		if (!TapBattleIOSBridge::IsReady())
		{
			FireError(OnError, TapBattleResultCode::INIT_FAIL,
				FString::Printf(TEXT("%s failed: TapBattle native SDK is not initialized"), Api));
			return false;
		}
		return true;
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

	FString MakeInitConfigJson()
	{
		const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();

		// 这些字段 TapSdkOnlineBattleInitialize 要求非空，任一为空都会让原生返回 -1
		// （初始化失败），随后 Connect 会返回 -2（未初始化）。逐字段构建并校验，
		// 把「具体哪个字段为空」直接打到日志里，避免只看到笼统的 -1/-2。
		//
		// dataDir 必须是绝对路径：FPaths::ProjectSavedDir() 在 iOS 真机上返回相对路径
		// （形如 ../../../TapSDKDemo/Saved/）。UE 自家文件 API 会把它重定向到沙盒，但原生
		// onlinebattle_sdk 走裸 POSIX/NSFileManager，按进程 CWD（沙盒外只读根）解析相对路径
		// 会落到不可写位置、建缓存目录失败，导致 Initialize 返回 -1。转成绝对路径并预建目录，
		// 与 CloudSave 给 native 传路径前先 ConvertRelativePathToFull 的做法一致。
		const FString DataDir = FPaths::ConvertRelativePathToFull(
			FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("onlinebattle_") + Config.ClientID));
		IFileManager::Get().MakeDirectory(*DataDir, /*Tree=*/true);
		const FString Ua = FString::Printf(TEXT("TapSDK-UE5/%s"), *FEngineVersion::Current().ToString());
		// FPlatformMisc::GetLoginId() 在 iOS 上可能为空（如 IDFV 暂不可用）。改用
		// 全 SDK 统一、保证非空且持久化的 FTapDeviceId，与 gatekeeper/openlog 对齐。
		const FString DeviceId = FTapDeviceId::Get();
		const FString NativeVersion = TapBattleIOSBridge::GetVersion();
		const FString SdkModuleVer = NativeVersion.IsEmpty() ? FString(TEXT("4.10.0")) : NativeVersion;

		auto WarnIfEmpty = [](const TCHAR* FieldName, const FString& Value)
		{
			if (Value.IsEmpty())
			{
				UE_LOG(LogTapBattle, Error,
					TEXT("[TapBattle][iOS] init config required field '%s' is empty; TapSdkOnlineBattleInitialize will fail"),
					FieldName);
			}
		};
		WarnIfEmpty(TEXT("dataDir"), DataDir);
		WarnIfEmpty(TEXT("ua"), Ua);
		WarnIfEmpty(TEXT("deviceId"), DeviceId);
		WarnIfEmpty(TEXT("sdkModuleVer"), SdkModuleVer);

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("region"), ComputeRegion(Config));
		Root->SetNumberField(TEXT("logToConsole"), Config.bEnableLog ? 1 : 0);
		Root->SetNumberField(TEXT("logLevel"), Config.bEnableLog ? 1 : 3);
		Root->SetStringField(TEXT("dataDir"), DataDir);
		Root->SetStringField(TEXT("ua"), Ua);
		Root->SetStringField(TEXT("lang"), LanguageToString(Config.PreferredLanguage));
		Root->SetStringField(TEXT("platform"), TEXT("iOS"));
		Root->SetStringField(TEXT("deviceId"), DeviceId);
		Root->SetStringField(TEXT("sdkArtifact"), TEXT("UE5"));
		Root->SetStringField(TEXT("sdkModuleVer"), SdkModuleVer);
		return JsonObjectToString(Root);
	}

	bool MakeConnectConfigJson(FString& OutJson, FString& OutError)
	{
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

	FString GetClientId()
	{
		return FTapSdkConfigState::GetCurrentConfig().ClientID;
	}
}

FTapBattlePlatformIOS::FTapBattlePlatformIOS() = default;

FTapBattlePlatformIOS::~FTapBattlePlatformIOS()
{
	TapBattleIOSBridge::Finalize();
}

void FTapBattlePlatformIOS::Initialize()
{
	if (bInitialized)
	{
		return;
	}

	const FString ConfigJson = MakeInitConfigJson();
	TWeakPtr<FTapBattlePlatformIOS> WeakSelf = AsShared();
	bInitialized = TapBattleIOSBridge::Init(ConfigJson, [WeakSelf](const FString& RawJson)
	{
		if (TSharedPtr<FTapBattlePlatformIOS> Self = WeakSelf.Pin())
		{
			Self->HandleNotification(RawJson);
		}
	});

	if (!bInitialized)
	{
		// 打印完整 config，便于定位非空字段以外的失败原因（如 dataDir 不可创建等）。
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][iOS] Initialize failed; config=%s"), *ConfigJson);
		return;
	}

	UE_LOG(LogTapBattle, Log, TEXT("[TapBattle][iOS] Initialize complete"));
}

bool FTapBattlePlatformIOS::IsInitialized() const
{
	return bInitialized;
}

void FTapBattlePlatformIOS::HandleNotification(const FString& RawJson)
{
	FBridgeEnvelope Envelope;
	if (!ParseEnvelope(RawJson, Envelope) || Envelope.Code != 0)
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][iOS] notification envelope parse failed: %s"), *RawJson);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	if (!ParseJsonObject(Envelope.Content, Root))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][iOS] notification content parse failed: %s"), *Envelope.Content);
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
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][iOS] unknown notification kind: %s"), *Kind);
		return;
	}

	FString EventName;
	Root->TryGetStringField(TEXT("event"), EventName);
	const TSharedPtr<FJsonObject> Data = GetObjectField(Root, TEXT("data"));
	if (!Data.IsValid())
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle][iOS] notification data missing for event: %s"), *EventName);
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

void FTapBattlePlatformIOS::SetLogLevel(int32 Level, bool bLogToConsole)
{
	TapBattleIOSBridge::SetLogLevel(Level, bLogToConsole);
}

FString FTapBattlePlatformIOS::GetNativeVersion()
{
	return TapBattleIOSBridge::GetVersion();
}

int64 FTapBattlePlatformIOS::NewRandomNumberGenerator(int32 Seed)
{
	return TapBattleIOSBridge::NewRandomNumberGenerator(Seed);
}

int32 FTapBattlePlatformIOS::RandomInt(int64 ObjId)
{
	return TapBattleIOSBridge::RandomInt(ObjId);
}

void FTapBattlePlatformIOS::FreeRandomNumberGenerator(int64 ObjId)
{
	TapBattleIOSBridge::FreeRandomNumberGenerator(ObjId);
}

void FTapBattlePlatformIOS::Connect(const FTapOnBattlePlayerIdResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!bInitialized)
	{
		const FString Message = TEXT("connect failed: TapBattle native SDK is not initialized");
		FireError(OnError, TapBattleResultCode::INIT_FAIL, Message);
		return;
	}

	FString ConfigJson;
	FString Error;
	if (!MakeConnectConfigJson(ConfigJson, Error))
	{
		FireError(OnError, TapBattleResultCode::NEED_LOGIN, Error);
		return;
	}

	TWeakPtr<FTapBattlePlatformIOS> WeakSelf = AsShared();
	TapBattleIOSBridge::Connect(ConfigJson, [WeakSelf, OnSuccess, OnError](bool bHasResult, const FString& Raw)
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

void FTapBattlePlatformIOS::Disconnect(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("disconnect"), OnError)) { return; }

	TapBattleIOSBridge::Disconnect(GetClientId(), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("disconnect"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::CreateRoom(const FTapBattleCreateRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("createRoom"), OnError)) { return; }

	TapBattleIOSBridge::CreateRoom(GetClientId(), SerializeCreateRoomConfig(Config), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
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

void FTapBattlePlatformIOS::MatchRoom(const FTapBattleMatchRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("matchRoom"), OnError)) { return; }

	TapBattleIOSBridge::MatchRoom(GetClientId(), SerializeMatchRoomConfig(Config), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
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

void FTapBattlePlatformIOS::GetRoomList(const FString& RoomType, int32 Offset, int32 Limit, const FTapOnBattleRoomListResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("getRoomList"), OnError)) { return; }

	TapBattleIOSBridge::GetRoomList(GetClientId(), SerializeGetRoomListRequest(RoomType, Offset, Limit), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
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

void FTapBattlePlatformIOS::JoinRoom(const FString& RoomId, const FTapBattlePlayerConfig& PlayerConfig, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("joinRoom"), OnError)) { return; }

	TapBattleIOSBridge::JoinRoom(GetClientId(), SerializeJoinRoomRequest(RoomId, PlayerConfig), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
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

void FTapBattlePlatformIOS::LeaveRoom(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("leaveRoom"), OnError)) { return; }

	TapBattleIOSBridge::LeaveRoom(GetClientId(), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("leaveRoom"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::UpdatePlayerCustomStatus(int32 Status, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("updatePlayerCustomStatus"), OnError)) { return; }

	TapBattleIOSBridge::UpdatePlayerCustomStatus(GetClientId(), Status, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updatePlayerCustomStatus"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::UpdatePlayerCustomProperties(const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("updatePlayerCustomProperties"), OnError)) { return; }

	TapBattleIOSBridge::UpdatePlayerCustomProperties(GetClientId(), Properties, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updatePlayerCustomProperties"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::UpdateRoomProperties(const FString& RoomName, const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("updateRoomProperties"), OnError)) { return; }

	TapBattleIOSBridge::UpdateRoomProperties(GetClientId(), SerializeUpdateRoomProperties(RoomName, Properties), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("updateRoomProperties"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::KickRoomPlayer(const FString& PlayerId, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("kickRoomPlayer"), OnError)) { return; }

	TapBattleIOSBridge::KickRoomPlayer(GetClientId(), PlayerId, [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("kickRoomPlayer"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::StartFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("startFrameSync"), OnError)) { return; }

	TapBattleIOSBridge::StartFrameSync(GetClientId(), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("startFrameSync"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::SendFrameInput(const FString& Data, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("sendFrameInput"), OnError)) { return; }

	TapBattleIOSBridge::SendFrameInput(GetClientId(), SerializeStringField(TEXT("data"), Data), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("sendFrameInput"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::StopFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("stopFrameSync"), OnError)) { return; }

	TapBattleIOSBridge::StopFrameSync(GetClientId(), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("stopFrameSync"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

void FTapBattlePlatformIOS::SendCustomMessage(const FString& Msg, int32 Type, const TArray<FString>& Receivers, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckReadyForNativeCall(TEXT("sendCustomMessage"), OnError)) { return; }

	TapBattleIOSBridge::SendCustomMessage(GetClientId(), SerializeCustomMessage(Msg, Type, Receivers), [OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		FBridgeEnvelope Envelope;
		if (!HandleEnvelopeOrFireError(TEXT("sendCustomMessage"), bHasResult, Raw, OnError, Envelope)) { return; }
		RunOnGameThread([OnSuccess] { OnSuccess.ExecuteIfBound(); });
	});
}

#endif // PLATFORM_IOS

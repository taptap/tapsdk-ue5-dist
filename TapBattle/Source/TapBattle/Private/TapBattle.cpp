// Copyright TapTap. All Rights Reserved.

#include "TapBattle.h"
#include "TapBattleModule.h"
#include "Platform/ITapBattlePlatform.h"
#include "TapOnlineBattleCallbackManager.h"
#include "Internal/TapSdkChecker.h"

// 与 Unity TapTapOnlineBattle.Version (4.10.0) 取齐；接 Native 后随 native SDK 版本统一升版
static const FString TapBattleVersion = TEXT("4.10.0");

namespace
{
	bool FireErrorIfFail(bool bCondition, int32 Code, const FString& Msg, const FTapOnBattleError& OnError)
	{
		if (bCondition) { return true; }
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle] Input check failed: %s"), *Msg);
		OnError.ExecuteIfBound(Code, Msg);
		return false;
	}

	bool CheckInitialize(const FTapOnBattleError& OnError)
	{
		if (FTapSdkChecker::CheckInitialize())
		{
			return true;
		}
		OnError.ExecuteIfBound(TapBattleResultCode::ERROR_NOT_INIT_OR_LOGIN, TEXT("TapSDK is not initialized"));
		return false;
	}

	TSharedPtr<ITapBattlePlatform> Platform()
	{
		auto P = FTapBattlePlatformFactory::CreatePlatform();
		if (!P.IsValid())
		{
			UE_LOG(LogTapBattle, Error, TEXT("[TapBattle] platform not available"));
		}
		return P;
	}
}

FString UTapBattle::GetVersion()
{
	if (auto P = Platform())
	{
		const FString NativeVersion = P->GetNativeVersion();
		if (!NativeVersion.IsEmpty())
		{
			return NativeVersion;
		}
	}
	return TapBattleVersion;
}

void UTapBattle::SetLogLevel(int32 Level, bool bLogToConsole)
{
	if (!FTapSdkChecker::CheckInitialize()) return;
	if (auto P = Platform()) { P->SetLogLevel(Level, bLogToConsole); }
}

TUniquePtr<FTapBattleRandomNumberGenerator> UTapBattle::NewRandomNumberGenerator(int32 Seed)
{
	if (!FTapSdkChecker::CheckInitialize()) return nullptr;
	auto P = Platform();
	if (!P.IsValid()) return nullptr;
	const int64 Handle = P->NewRandomNumberGenerator(Seed);
	if (Handle == 0 || Handle == -1) return nullptr;
	return TUniquePtr<FTapBattleRandomNumberGenerator>(new FTapBattleRandomNumberGenerator(P, Handle));
}

// ---------- 全局回调 ----------

void UTapBattle::RegisterOnlineBattleCallback(TScriptInterface<ITapOnlineBattleCallback> Callback)
{
	if (!FTapSdkChecker::CheckInitialize()) return;
	if (!IsValid(Callback.GetObject()) ||
		!Callback.GetObject()->GetClass()->ImplementsInterface(UTapOnlineBattleCallback::StaticClass()))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle] RegisterOnlineBattleCallback: callback is invalid"));
		return;
	}
	FTapOnlineBattleCallbackManager::Get().Register(Callback);
}

void UTapBattle::UnregisterOnlineBattleCallback(TScriptInterface<ITapOnlineBattleCallback> Callback)
{
	if (!FTapSdkChecker::CheckInitialize()) return;
	if (!IsValid(Callback.GetObject()) ||
		!Callback.GetObject()->GetClass()->ImplementsInterface(UTapOnlineBattleCallback::StaticClass()))
	{
		UE_LOG(LogTapBattle, Warning, TEXT("[TapBattle] UnregisterOnlineBattleCallback: callback is invalid"));
		return;
	}
	FTapOnlineBattleCallbackManager::Get().Unregister(Callback);
}

// ---------- 连接 ----------

void UTapBattle::Connect(const FTapOnBattlePlayerIdResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->Connect(OnSuccess, OnError);
}

void UTapBattle::Disconnect(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->Disconnect(OnSuccess, OnError);
}

// ---------- 房间 ----------

void UTapBattle::CreateRoom(const FTapBattleCreateRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	if (!FireErrorIfFail(Config.RoomConfig.MaxPlayerCount > 0, TapBattleResultCode::INVALID_PARAM, TEXT("RoomConfig.MaxPlayerCount must be > 0"), OnError)) return;
	if (!FireErrorIfFail(!Config.RoomConfig.Type.IsEmpty(),    TapBattleResultCode::INVALID_PARAM, TEXT("RoomConfig.Type required"),               OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->CreateRoom(Config, OnSuccess, OnError);
}

void UTapBattle::MatchRoom(const FTapBattleMatchRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	if (!FireErrorIfFail(Config.RoomConfig.MaxPlayerCount > 0, TapBattleResultCode::INVALID_PARAM, TEXT("RoomConfig.MaxPlayerCount must be > 0"), OnError)) return;
	if (!FireErrorIfFail(!Config.RoomConfig.Type.IsEmpty(),    TapBattleResultCode::INVALID_PARAM, TEXT("RoomConfig.Type required"),               OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->MatchRoom(Config, OnSuccess, OnError);
}

void UTapBattle::GetRoomList(const FString& RoomType, int32 Offset, int32 Limit, const FTapOnBattleRoomListResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	if (!FireErrorIfFail(Offset >= 0,         TapBattleResultCode::INVALID_PARAM, TEXT("Offset must be >= 0"),               OnError)) return;
	if (!FireErrorIfFail(Limit  >  0,         TapBattleResultCode::INVALID_PARAM, TEXT("Limit must be > 0"),                 OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->GetRoomList(RoomType, Offset, Limit, OnSuccess, OnError);
}

void UTapBattle::JoinRoom(const FString& RoomId, const FTapBattlePlayerConfig& PlayerConfig, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->JoinRoom(RoomId, PlayerConfig, OnSuccess, OnError);
}

void UTapBattle::LeaveRoom(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->LeaveRoom(OnSuccess, OnError);
}

// ---------- 玩家 / 房间属性 ----------

void UTapBattle::UpdatePlayerCustomStatus(int32 Status, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->UpdatePlayerCustomStatus(Status, OnSuccess, OnError);
}

void UTapBattle::UpdatePlayerCustomProperties(const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->UpdatePlayerCustomProperties(Properties, OnSuccess, OnError);
}

void UTapBattle::UpdateRoomProperties(const FString& RoomName, const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->UpdateRoomProperties(RoomName, Properties, OnSuccess, OnError);
}

void UTapBattle::KickRoomPlayer(const FString& PlayerId, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->KickRoomPlayer(PlayerId, OnSuccess, OnError);
}

// ---------- 帧同步 ----------

void UTapBattle::StartFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->StartFrameSync(OnSuccess, OnError);
}

void UTapBattle::SendFrameInput(const FString& Data, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->SendFrameInput(Data, OnSuccess, OnError);
}

void UTapBattle::StopFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->StopFrameSync(OnSuccess, OnError);
}

// ---------- 自定义消息 ----------

void UTapBattle::SendCustomMessage(const FString& Msg, int32 Type, const TArray<FString>& Receivers, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError)
{
	if (!CheckInitialize(OnError)) return;
	if (!FireErrorIfFail(!Msg.IsEmpty(), TapBattleResultCode::INVALID_PARAM, TEXT("Msg required"),                                          OnError)) return;
	if (!FireErrorIfFail(Type == 0 || Type == 1, TapBattleResultCode::INVALID_PARAM, TEXT("Type must be 0 (broadcast) or 1 (specific)"),    OnError)) return;
	if (Type == 1 && !FireErrorIfFail(Receivers.Num() > 0, TapBattleResultCode::INVALID_PARAM, TEXT("Receivers required when Type == 1"),   OnError)) return;
	if (!FireErrorIfFail(Receivers.Num() <= 20, TapBattleResultCode::INVALID_PARAM, TEXT("Receivers cannot exceed 20 ids"),                 OnError)) return;
	auto P = Platform();
	if (!FireErrorIfFail(P.IsValid(), TapBattleResultCode::PLATFORM_NOT_AVAILABLE, TEXT("platform not implemented"), OnError)) return;
	P->SendCustomMessage(Msg, Type, Receivers, OnSuccess, OnError);
}

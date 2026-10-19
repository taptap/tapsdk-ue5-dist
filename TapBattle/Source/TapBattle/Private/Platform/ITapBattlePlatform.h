// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapBattleTypes.h"

class ITapBattlePlatform
{
public:
	virtual ~ITapBattlePlatform() = default;

	virtual void    Initialize() = 0;
	virtual bool    IsInitialized() const = 0;
	virtual FString GetPlatformName() const = 0;

	virtual void SetLogLevel(int32 Level, bool bLogToConsole) = 0;
	virtual FString GetNativeVersion() = 0;
	virtual int64 NewRandomNumberGenerator(int32 Seed) = 0;
	virtual int32 RandomInt(int64 ObjId) = 0;
	virtual void FreeRandomNumberGenerator(int64 ObjId) = 0;

	// 连接
	virtual void Connect(const FTapOnBattlePlayerIdResult& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void Disconnect(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;

	// 房间
	virtual void CreateRoom(const FTapBattleCreateRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void MatchRoom(const FTapBattleMatchRoomConfig& Config, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void GetRoomList(const FString& RoomType, int32 Offset, int32 Limit, const FTapOnBattleRoomListResult& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void JoinRoom(const FString& RoomId, const FTapBattlePlayerConfig& PlayerConfig, const FTapOnBattleRoomResult& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void LeaveRoom(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;

	// 玩家/房间属性
	virtual void UpdatePlayerCustomStatus(int32 Status, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void UpdatePlayerCustomProperties(const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void UpdateRoomProperties(const FString& RoomName, const FString& Properties, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void KickRoomPlayer(const FString& PlayerId, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;

	// 帧同步
	virtual void StartFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void SendFrameInput(const FString& Data, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;
	virtual void StopFrameSync(const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;

	// 自定义消息
	virtual void SendCustomMessage(const FString& Msg, int32 Type, const TArray<FString>& Receivers, const FTapOnBattleSuccess& OnSuccess, const FTapOnBattleError& OnError) = 0;
};

class FTapBattlePlatformFactory
{
public:
	static TSharedPtr<ITapBattlePlatform> CreatePlatform();
#if WITH_DEV_AUTOMATION_TESTS
	static void                           SetPlatformForTest(TSharedPtr<ITapBattlePlatform> InTest);
#endif

	// Release the cached platform instance. Called from FTapBattleModule::
	// ShutdownModule so any future native handles are freed on module reload
	// (Editor live coding, plugin disable/enable).
	static void                           Reset();

private:
	static TSharedPtr<ITapBattlePlatform> PlatformInstance;
#if WITH_DEV_AUTOMATION_TESTS
	static TSharedPtr<ITapBattlePlatform> TestInstance;
#endif
};

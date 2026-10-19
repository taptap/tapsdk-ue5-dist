// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapBattleTypes.h"
#include "TapBattleRandomNumberGenerator.h"
#include "TapOnlineBattleCallback.h"
#include "TapBattle.generated.h"

/**
 * TapBattle 静态门面 API（C++ + Blueprint 双轨）
 * 对齐 Unity TapTapOnlineBattle（17 个公共方法 + 全局回调）
 *
 * Android 走 TapTapBattle Router；iOS / Desktop 走 OnlineBattle C ABI。
 */
UCLASS()
class TAPBATTLE_API UTapBattle : public UObject
{
	GENERATED_BODY()

public:
	static FString GetVersion();

	// Android 原生 SDK 日志等级：1 trace / 2 debug / 3 info / 4 warn / 5 error
	static void SetLogLevel(int32 Level, bool bLogToConsole = true);

	// 使用帧同步开始事件返回的 Seed 创建确定性随机数生成器。
	// 返回拥有原生资源的对象；创建失败返回 nullptr。可显式 Free，析构也会释放。
	static TUniquePtr<FTapBattleRandomNumberGenerator> NewRandomNumberGenerator(int32 Seed);

	// ----- 全局回调（对齐 Android/Unity OnlineBattleCallback）-----

	UFUNCTION(BlueprintCallable, Category="TapOnlineBattle")
	static void RegisterOnlineBattleCallback(TScriptInterface<ITapOnlineBattleCallback> Callback);

	UFUNCTION(BlueprintCallable, Category="TapOnlineBattle")
	static void UnregisterOnlineBattleCallback(TScriptInterface<ITapOnlineBattleCallback> Callback);

	// ----- 连接 -----
	static void Connect(
		const FTapOnBattlePlayerIdResult& OnSuccess,
		const FTapOnBattleError&          OnError);

	static void Disconnect(
		const FTapOnBattleSuccess& OnSuccess,
		const FTapOnBattleError&   OnError);

	// ----- 房间 -----
	static void CreateRoom(
		const FTapBattleCreateRoomConfig& Config,
		const FTapOnBattleRoomResult&     OnSuccess,
		const FTapOnBattleError&          OnError);

	static void MatchRoom(
		const FTapBattleMatchRoomConfig& Config,
		const FTapOnBattleRoomResult&    OnSuccess,
		const FTapOnBattleError&         OnError);

	static void GetRoomList(
		const FString& RoomType,
		int32          Offset,
		int32          Limit,
		const FTapOnBattleRoomListResult& OnSuccess,
		const FTapOnBattleError&          OnError);

	static void JoinRoom(
		const FString&                RoomId,
		const FTapBattlePlayerConfig& PlayerConfig,
		const FTapOnBattleRoomResult& OnSuccess,
		const FTapOnBattleError&      OnError);

	static void LeaveRoom(
		const FTapOnBattleSuccess& OnSuccess,
		const FTapOnBattleError&   OnError);

	// ----- 玩家 / 房间属性 -----
	static void UpdatePlayerCustomStatus(
		int32                       Status,
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);

	static void UpdatePlayerCustomProperties(
		const FString&              Properties,
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);

	static void UpdateRoomProperties(
		const FString&              RoomName,
		const FString&              Properties,
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);

	static void KickRoomPlayer(
		const FString&              PlayerId,
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);

	// ----- 帧同步 -----
	static void StartFrameSync(
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);

	static void SendFrameInput(
		const FString&              Data,
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);

	static void StopFrameSync(
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);

	// ----- 自定义消息 -----
	// Type: 0 房间内全部（不含发送者）；1 发送给指定玩家
	static void SendCustomMessage(
		const FString&              Msg,
		int32                       Type,
		const TArray<FString>&      Receivers,
		const FTapOnBattleSuccess&  OnSuccess,
		const FTapOnBattleError&    OnError);
};

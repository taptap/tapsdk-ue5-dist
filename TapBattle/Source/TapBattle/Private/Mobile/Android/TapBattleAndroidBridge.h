// Copyright TapTap. All Rights Reserved.

#pragma once

#if PLATFORM_ANDROID

#include "CoreMinimal.h"

class TapBattleAndroidBridge
{
public:
	using FResultCallback = TFunction<void(bool bHasResult, const FString& RawJson)>;

	static void Init();

	static bool RegisterNotificationCallback(TFunction<void(const FString&)> OnNotification);
	static void UnregisterNotificationCallback();

	static void SetLogLevel(int32 Level, bool bLogToConsole);
	static FString GetVersion();
	static int64 NewRandomNumberGenerator(int32 Seed);
	static int32 RandomInt(int64 ObjId);
	static void FreeRandomNumberGenerator(int64 ObjId);

	static void Connect(FResultCallback OnResult);
	static void Disconnect(FResultCallback OnResult);

	static void CreateRoom(const FString& ParamsJson, FResultCallback OnResult);
	static void MatchRoom(const FString& ParamsJson, FResultCallback OnResult);
	static void GetRoomList(const FString& ParamsJson, FResultCallback OnResult);
	static void JoinRoom(const FString& ParamsJson, FResultCallback OnResult);
	static void LeaveRoom(FResultCallback OnResult);

	static void UpdatePlayerCustomStatus(int32 Status, FResultCallback OnResult);
	static void UpdatePlayerCustomProperties(const FString& Properties, FResultCallback OnResult);
	static void UpdateRoomProperties(const FString& RoomName, const FString& Properties, FResultCallback OnResult);
	static void KickRoomPlayer(const FString& PlayerId, FResultCallback OnResult);

	static void StartFrameSync(FResultCallback OnResult);
	static void SendFrameInput(const FString& Data, FResultCallback OnResult);
	static void StopFrameSync(FResultCallback OnResult);

	static void SendCustomMessage(const FString& Msg, int32 Type, const TArray<FString>& Receivers, FResultCallback OnResult);
};

#endif // PLATFORM_ANDROID

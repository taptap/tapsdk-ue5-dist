// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

namespace TapBattleDesktopBridge
{
	using FResultCallback = TFunction<void(bool /*bHasResult*/, const FString& /*ResultJson*/)>;
	using FNotificationCallback = TFunction<void(const FString& /*ResultJson*/)>;

	bool Init(const FString& ConfigJson, FNotificationCallback OnNotification);
	void Finalize();
	bool IsReady();

	void SetLogLevel(int32 Level, bool bLogToConsole);
	FString GetVersion();
	FString GetGitCommit();
	int64 NewRandomNumberGenerator(int32 Seed);
	int32 RandomInt(int64 ObjId);
	void FreeRandomNumberGenerator(int64 ObjId);

	void Connect(const FString& ConfigJson, FResultCallback OnResult);
	void Disconnect(const FString& ClientId, FResultCallback OnResult);

	void CreateRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult);
	void MatchRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult);
	void GetRoomList(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult);
	void JoinRoom(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult);
	void LeaveRoom(const FString& ClientId, FResultCallback OnResult);

	void UpdatePlayerCustomStatus(const FString& ClientId, int32 Status, FResultCallback OnResult);
	void UpdatePlayerCustomProperties(const FString& ClientId, const FString& Properties, FResultCallback OnResult);
	void UpdateRoomProperties(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult);
	void KickRoomPlayer(const FString& ClientId, const FString& PlayerId, FResultCallback OnResult);

	void StartFrameSync(const FString& ClientId, FResultCallback OnResult);
	void SendFrameInput(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult);
	void StopFrameSync(const FString& ClientId, FResultCallback OnResult);

	void SendCustomMessage(const FString& ClientId, const FString& ParamsJson, FResultCallback OnResult);
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

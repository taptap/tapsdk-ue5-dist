// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "TapOnlineBattleCallback.h"

/** Stores developer callbacks weakly and dispatches every native notification on the game thread. */
class FTapOnlineBattleCallbackManager
{
public:
	static FTapOnlineBattleCallbackManager& Get();

	void Register(const TScriptInterface<ITapOnlineBattleCallback>& Callback);
	void Unregister(const TScriptInterface<ITapOnlineBattleCallback>& Callback);
	void Reset();

	void DispatchDisconnected(int32 Code, const FString& Message);
	void DispatchBattleServiceError();
	void DispatchRoomPropertiesChanged(const FString& RoomId, const FString& Name, const FString& CustomProperties);
	void DispatchPlayerCustomPropertiesChanged(const FString& PlayerId, const FString& Properties);
	void DispatchPlayerCustomStatusChanged(const FString& PlayerId, int32 Status);
	void DispatchFrameSyncStarted(const FTapBattleFrameSyncInfo& FrameSyncInfo);
	void DispatchFrameReceived(const FTapBattleFrameData& FrameData);
	void DispatchFrameSyncStopped(const FString& RoomId, int32 FrameSyncId, int32 Reason);
	void DispatchPlayerOffline(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId);
	void DispatchPlayerLeft(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId);
	void DispatchPlayerEntered(const FString& RoomId, const FTapBattlePlayerInfo& PlayerInfo);
	void DispatchCustomMessageReceived(const FString& PlayerId, const FString& Message);
	void DispatchPlayerKicked(const FString& RoomId, const FString& PlayerId);

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetRegisteredCallbackCountForTest();
#endif

private:
	void Dispatch(TFunction<void(UObject*)> Invocation);
	void DispatchOnGameThread(TFunction<void(UObject*)> Invocation);

	FCriticalSection CallbackLock;
	TArray<TWeakObjectPtr<UObject>> Callbacks;
	uint64 Generation = 0;
};

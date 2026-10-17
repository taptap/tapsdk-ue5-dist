// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapBattleTypes.h"
#include "TapOnlineBattleCallback.generated.h"

/**
 * Online Battle global notification callback.
 *
 * The method set mirrors Android TapOnlineBattleCallback and Unity
 * ITapOnlineBattleCallback. C++ implementations override the generated
 * *_Implementation methods; Blueprint implementations override the events.
 */
UINTERFACE(BlueprintType)
class TAPBATTLE_API UTapOnlineBattleCallback : public UInterface
{
	GENERATED_BODY()
};

class TAPBATTLE_API ITapOnlineBattleCallback
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnDisconnected(int32 Code, const FString& Message);
	virtual void OnDisconnected_Implementation(int32 Code, const FString& Message) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnBattleServiceError();
	virtual void OnBattleServiceError_Implementation() {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnRoomPropertiesChanged(const FString& RoomId, const FString& Name, const FString& CustomProperties);
	virtual void OnRoomPropertiesChanged_Implementation(const FString& RoomId, const FString& Name, const FString& CustomProperties) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnPlayerCustomPropertiesChanged(const FString& PlayerId, const FString& Properties);
	virtual void OnPlayerCustomPropertiesChanged_Implementation(const FString& PlayerId, const FString& Properties) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnPlayerCustomStatusChanged(const FString& PlayerId, int32 Status);
	virtual void OnPlayerCustomStatusChanged_Implementation(const FString& PlayerId, int32 Status) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnFrameSyncStarted(const FTapBattleFrameSyncInfo& FrameSyncInfo);
	virtual void OnFrameSyncStarted_Implementation(const FTapBattleFrameSyncInfo& FrameSyncInfo) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnFrameReceived(const FTapBattleFrameData& FrameData);
	virtual void OnFrameReceived_Implementation(const FTapBattleFrameData& FrameData) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnFrameSyncStopped(const FString& RoomId, int32 FrameSyncId, int32 Reason);
	virtual void OnFrameSyncStopped_Implementation(const FString& RoomId, int32 FrameSyncId, int32 Reason) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnPlayerOffline(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId);
	virtual void OnPlayerOffline_Implementation(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnPlayerLeft(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId);
	virtual void OnPlayerLeft_Implementation(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnPlayerEntered(const FString& RoomId, const FTapBattlePlayerInfo& PlayerInfo);
	virtual void OnPlayerEntered_Implementation(const FString& RoomId, const FTapBattlePlayerInfo& PlayerInfo) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnCustomMessageReceived(const FString& PlayerId, const FString& Message);
	virtual void OnCustomMessageReceived_Implementation(const FString& PlayerId, const FString& Message) {}

	UFUNCTION(BlueprintNativeEvent, Category="TapOnlineBattle")
	void OnPlayerKicked(const FString& RoomId, const FString& PlayerId);
	virtual void OnPlayerKicked_Implementation(const FString& RoomId, const FString& PlayerId) {}
};

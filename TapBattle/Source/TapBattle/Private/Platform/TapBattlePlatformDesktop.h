// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Platform/ITapBattlePlatform.h"
#include "Internal/Platform/TapOpenlogTracker.h"

// Desktop (Win64 / Mac) implementation.
//
// Mirrors Unity OnlineBattle PC: directly loads onlinebattle_sdk and calls the
// TapSdkOnlineBattle* C ABI. Windows business calls require the TapTap PC
// launcher check to have passed, matching Unity's TapClientStandalone gate.
class FTapBattlePlatformDesktop
	: public ITapBattlePlatform
	, public TSharedFromThis<FTapBattlePlatformDesktop>
{
public:
	FTapBattlePlatformDesktop();
	virtual ~FTapBattlePlatformDesktop();

	virtual void    Initialize() override;
	virtual bool    IsInitialized() const override;
	virtual FString GetPlatformName() const override { return TEXT("Desktop"); }

	virtual void SetLogLevel(int32 Level, bool bLogToConsole) override;
	virtual FString GetNativeVersion() override;
	virtual int64 NewRandomNumberGenerator(int32 Seed) override;
	virtual int32 RandomInt(int64 ObjId) override;
	virtual void FreeRandomNumberGenerator(int64 ObjId) override;

	virtual void Connect(const FTapOnBattlePlayerIdResult&, const FTapOnBattleError&) override;
	virtual void Disconnect(const FTapOnBattleSuccess&, const FTapOnBattleError&) override;

	virtual void CreateRoom(const FTapBattleCreateRoomConfig&, const FTapOnBattleRoomResult&, const FTapOnBattleError&) override;
	virtual void MatchRoom (const FTapBattleMatchRoomConfig&,  const FTapOnBattleRoomResult&, const FTapOnBattleError&) override;
	virtual void GetRoomList(const FString&, int32, int32, const FTapOnBattleRoomListResult&, const FTapOnBattleError&) override;
	virtual void JoinRoom (const FString&, const FTapBattlePlayerConfig&, const FTapOnBattleRoomResult&, const FTapOnBattleError&) override;
	virtual void LeaveRoom(const FTapOnBattleSuccess&, const FTapOnBattleError&) override;

	virtual void UpdatePlayerCustomStatus    (int32, const FTapOnBattleSuccess&, const FTapOnBattleError&) override;
	virtual void UpdatePlayerCustomProperties(const FString&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override;
	virtual void UpdateRoomProperties        (const FString&, const FString&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override;
	virtual void KickRoomPlayer              (const FString&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override;

	virtual void StartFrameSync(const FTapOnBattleSuccess&, const FTapOnBattleError&) override;
	virtual void SendFrameInput(const FString&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override;
	virtual void StopFrameSync (const FTapOnBattleSuccess&, const FTapOnBattleError&) override;

	virtual void SendCustomMessage(const FString&, int32, const TArray<FString>&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override;

private:
	void HandleNotification(const FString& RawJson);

	bool bInitialized = false;

	FTapOpenlogTracker Tracker{TEXT("TapOnlineBattle"), TEXT("")};
};

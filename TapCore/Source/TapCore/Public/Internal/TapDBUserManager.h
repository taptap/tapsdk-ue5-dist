// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Containers/Ticker.h"
#include "Internal/TapDBConstants.h"

class FTapMachineBoundPrefs;
class FTapDBEventSender;

DECLARE_DELEGATE_TwoParams(FOnPlayGameEvent, const FString& /*UserID*/, int64 /*Duration*/);

/**
 * Manages user sessions and play time tracking.
 * On login, starts recording play duration.
 * On logout, reports play_game event with accumulated duration.
 */
class TAPCORE_API FTapDBUserManager
{
public:
	FTapDBUserManager();
	~FTapDBUserManager();

	void Init(FTapMachineBoundPrefs& InPrefs, bool bPrivateSession = false);
	void Login(const FString& UserID, TSharedPtr<FJsonObject> Properties = nullptr);
	void Logout();
	void Shutdown();

	FString GetCurrentUserID() const { return CurrentUserID; }
	bool HasUser() const { return !CurrentUserID.IsEmpty(); }

	/** Called when play_game event should be sent. Tracker registers this callback. */
	FOnPlayGameEvent OnPlayGameEvent;
	TFunction<TSharedPtr<FJsonObject>(const FString&, int64)> BuildDurationSnapshot;
	TFunction<void(TSharedPtr<FJsonObject>)> SendDurationSnapshot;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTapIsolatedDurationTest;
#endif
	/** Returns a per-user storage key for played duration, e.g. "tapdb_played_duration_UserID123" */
	FString PlayedDurationKey() const
	{
		return FString::Printf(TEXT("%s_%s"), *TapDBConstants::PREF_PLAYED_DURATION, *CurrentUserID);
	}

	void StartPlayRecording();
	void StopPlayRecording();
	bool OnPlayTimerTick(float DeltaTime);

	bool bIsolated = false;
	uint64 SessionGeneration = 0;
	FString CurrentUserID;
	int64 PlayedDuration;
	FTapMachineBoundPrefs* Prefs;

	FTSTicker::FDelegateHandle PlayTickerHandle;
};

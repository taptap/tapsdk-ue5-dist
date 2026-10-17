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

	void Init(FTapMachineBoundPrefs& InPrefs);
	void Login(const FString& UserID, TSharedPtr<FJsonObject> Properties = nullptr);
	void Logout();
	void Shutdown();

	FString GetCurrentUserID() const { return CurrentUserID; }
	bool HasUser() const { return !CurrentUserID.IsEmpty(); }

	/** Called when play_game event should be sent. Tracker registers this callback. */
	FOnPlayGameEvent OnPlayGameEvent;

private:
	/** Returns a per-user storage key for played duration, e.g. "tapdb_played_duration_UserID123" */
	FString PlayedDurationKey() const
	{
		return FString::Printf(TEXT("%s_%s"), *TapDBConstants::PREF_PLAYED_DURATION, *CurrentUserID);
	}

	void StartPlayRecording();
	void StopPlayRecording();
	bool OnPlayTimerTick(float DeltaTime);

	FString CurrentUserID;
	int64 PlayedDuration;
	FTapMachineBoundPrefs* Prefs;

	FTSTicker::FDelegateHandle PlayTickerHandle;
};

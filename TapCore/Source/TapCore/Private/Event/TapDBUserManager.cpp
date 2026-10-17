// Copyright TapTap. All Rights Reserved.

#include "Internal/TapDBUserManager.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/TapDBConstants.h"
#include "TapCoreModule.h"

FTapDBUserManager::FTapDBUserManager()
	: PlayedDuration(0)
	, Prefs(nullptr)
{
}

FTapDBUserManager::~FTapDBUserManager()
{
	Shutdown();
}

void FTapDBUserManager::Init(FTapMachineBoundPrefs& InPrefs)
{
	Prefs = &InPrefs;

	// User identity is session-scoped. A persisted user id is only applied
	// when SetUserID is called again this session, so idle init cannot emit
	// user_login / has_user or attach a previous user to device_login.
}

void FTapDBUserManager::Login(const FString& UserID, TSharedPtr<FJsonObject> Properties)
{
	if (UserID.IsEmpty())
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBUserManager] Login called with empty UserID"));
		return;
	}

	// Logout previous user if different
	if (!CurrentUserID.IsEmpty() && CurrentUserID != UserID)
	{
		Logout();
	}

	CurrentUserID = UserID;
	PlayedDuration = Prefs ? Prefs->GetInt64(PlayedDurationKey(), 0) : 0;

	if (Prefs)
	{
		Prefs->Set(TapDBConstants::PREF_USER_ID, CurrentUserID);
		Prefs->SetInt64(PlayedDurationKey(), PlayedDuration);
		Prefs->Save();
	}

	StartPlayRecording();

	UE_LOG(LogTapDB, Log, TEXT("[TapDBUserManager] User logged in: %s"), *UserID);
}

void FTapDBUserManager::Logout()
{
	if (CurrentUserID.IsEmpty())
	{
		return;
	}

	StopPlayRecording();

	FString LogoutUserID = CurrentUserID;
	int64 Duration = PlayedDuration;

	// Clear this user's persisted duration
	if (Prefs)
	{
		Prefs->Remove(PlayedDurationKey());
		Prefs->Remove(TapDBConstants::PREF_USER_ID);
		Prefs->Save();
	}

	CurrentUserID.Empty();
	PlayedDuration = 0;

	// Fire play_game event with accumulated duration
	if (Duration > 0)
	{
		OnPlayGameEvent.ExecuteIfBound(LogoutUserID, Duration);
	}

	UE_LOG(LogTapDB, Log, TEXT("[TapDBUserManager] User logged out: %s, duration: %lld s"), *LogoutUserID, Duration);
}

void FTapDBUserManager::Shutdown()
{
	StopPlayRecording();

	// Persist current state
	if (Prefs && !CurrentUserID.IsEmpty())
	{
		Prefs->Set(TapDBConstants::PREF_USER_ID, CurrentUserID);
		Prefs->SetInt64(PlayedDurationKey(), PlayedDuration);
		Prefs->Save();
	}
	Prefs = nullptr;
}

void FTapDBUserManager::StartPlayRecording()
{
	// Cancel existing ticker
	if (PlayTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PlayTickerHandle);
		PlayTickerHandle.Reset();
	}

	PlayTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FTapDBUserManager::OnPlayTimerTick),
		TapDBConstants::PLAY_RECORD_INTERVAL_SECONDS
	);
}

void FTapDBUserManager::StopPlayRecording()
{
	if (PlayTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PlayTickerHandle);
		PlayTickerHandle.Reset();
	}
}

bool FTapDBUserManager::OnPlayTimerTick(float DeltaTime)
{
	if (CurrentUserID.IsEmpty())
	{
		return false; // Stop ticking
	}

	PlayedDuration += static_cast<int64>(TapDBConstants::PLAY_RECORD_INTERVAL_SECONDS);

	// Persist periodically
	if (Prefs)
	{
		Prefs->SetInt64(PlayedDurationKey(), PlayedDuration);
		// Save every ~10s (every 5 ticks) to reduce disk I/O
		if (PlayedDuration % 10 == 0)
		{
			Prefs->Save();
		}
	}

	return true; // Keep ticking
}

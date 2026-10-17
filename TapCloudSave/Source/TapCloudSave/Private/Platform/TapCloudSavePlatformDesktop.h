// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "Platform/ITapCloudSavePlatform.h"
#include "Internal/Platform/TapOpenlogTracker.h"
#include "TapLogin.h"

// Inherit from TSharedFromThis so async lambdas can capture a weak self
// (WeakSelf.Pin()) instead of a raw `this`, eliminating the use-after-free
// risk when the platform instance is reset (e.g. ShutdownModule).
class FTapCloudSavePlatformDesktop
	: public ITapCloudSavePlatform
	, public TSharedFromThis<FTapCloudSavePlatformDesktop>
{
public:
	FTapCloudSavePlatformDesktop();
	virtual ~FTapCloudSavePlatformDesktop();

	virtual void Initialize() override;
	virtual bool IsInitialized() const override;
	virtual FString GetPlatformName() const override { return TEXT("Desktop"); }

	virtual void RegisterStateCallback(const FTapOnCloudSaveState& OnState) override;
	virtual void UnregisterStateCallback(const FTapOnCloudSaveState& OnState) override;

	virtual void CreateArchive(const FTapArchiveMetadata&, const FString&, const FString&, const FTapOnArchiveResult&, const FTapOnArchiveError&) override;
	virtual void UpdateArchive(const FString&, const FTapArchiveMetadata&, const FString&, const FString&, const FTapOnArchiveResult&, const FTapOnArchiveError&) override;
	virtual void DeleteArchive(const FString&, const FTapOnArchiveResult&, const FTapOnArchiveError&) override;
	virtual void GetArchiveList(const FTapOnArchiveListResult&, const FTapOnArchiveError&) override;
	virtual void GetArchiveData (const FString&, const FString&, const FTapOnArchiveBinaryResult&, const FTapOnArchiveError&) override;
	virtual void GetArchiveCover(const FString&, const FString&, const FTapOnArchiveBinaryResult&, const FTapOnArchiveError&) override;

private:
	bool CheckInitAndLogin(FString& OutErrorMsg, int32& OutErrorCode);
	bool WaitForNativeInitialization();
	bool HasCompleteLoginCredentials(const TOptional<FTapAccount>& Account) const;
	void NotifyState(ETapCloudSaveState State, int32 RawCode);
	bool SyncAccessToken();
	void HandleLoginStateChanged(const TOptional<FTapAccount>& Account);
	bool PushAccessToken();
	void SeedSyncedToken(const TOptional<FTapAccount>& Account);
	void ResetSyncedToken();

	TArray<FTapOnCloudSaveState> StateCallbacks;
	FCriticalSection             StateLock;

	mutable FCriticalSection     InitLock;
	bool                         bInitNative = false;
	bool                         bInitStarted = false;
	bool                         bInitComplete = false;
	// Signalled after the asynchronous initialisation attempt completes, so API
	// calls can wait for the one in-flight attempt instead of reporting 300002.
	FEvent*                      InitCompleteEvent = nullptr;

	FCriticalSection             TokenPushLock;
	FCriticalSection             TokenLock;
	FString                      SyncedTokenKid;
	FString                      SyncedMacKey;

	FTapOpenlogTracker           Tracker{TEXT("TapCloudSave"), TEXT("")};
};

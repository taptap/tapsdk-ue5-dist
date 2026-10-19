// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Platform/ITapCloudSavePlatform.h"

// iOS-specific platform implementation. Routes every call through
// TapCloudSaveIOSBridge → BridgeCloudSaveService (Swift/ObjC SDK).
//
// Inherits TSharedFromThis so async callback blocks can capture a
// TWeakPtr<Self> and safely no-op after ShutdownModule resets the cached
// factory instance (mirrors the Desktop platform's pattern).
class FTapCloudSavePlatformIOS
	: public ITapCloudSavePlatform
	, public TSharedFromThis<FTapCloudSavePlatformIOS>
{
public:
	FTapCloudSavePlatformIOS();
	virtual ~FTapCloudSavePlatformIOS();

	virtual void Initialize() override;
	virtual bool IsInitialized() const override;
	virtual FString GetPlatformName() const override { return TEXT("iOS"); }

	virtual void RegisterStateCallback(const FTapOnCloudSaveState& OnState) override;
	virtual void UnregisterStateCallback(const FTapOnCloudSaveState& OnState) override;

	virtual void CreateArchive(const FTapArchiveMetadata&, const FString&, const FString&, const FTapOnArchiveResult&, const FTapOnArchiveError&) override;
	virtual void UpdateArchive(const FString&, const FTapArchiveMetadata&, const FString&, const FString&, const FTapOnArchiveResult&, const FTapOnArchiveError&) override;
	virtual void DeleteArchive(const FString&, const FTapOnArchiveResult&, const FTapOnArchiveError&) override;
	virtual void GetArchiveList(const FTapOnArchiveListResult&, const FTapOnArchiveError&) override;
	virtual void GetArchiveData (const FString&, const FString&, const FTapOnArchiveBinaryResult&, const FTapOnArchiveError&) override;
	virtual void GetArchiveCover(const FString&, const FString&, const FTapOnArchiveBinaryResult&, const FTapOnArchiveError&) override;

private:
	void NotifyState(ETapCloudSaveState State, int32 RawCode);
	void HandleRawState(int32 RawCode);

	TArray<FTapOnCloudSaveState> StateCallbacks;
	FCriticalSection             StateLock;

	bool                         bInitialized = false;
};

// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapCloudSaveTypes.h"
#include "TapCloudSaveInternalTypes.h"

class ITapCloudSavePlatform
{
public:
	virtual ~ITapCloudSavePlatform() = default;

	virtual void Initialize() = 0;
	virtual bool IsInitialized() const = 0;
	virtual FString GetPlatformName() const = 0;

	virtual void RegisterStateCallback(const FTapOnCloudSaveState& OnState) = 0;
	virtual void UnregisterStateCallback(const FTapOnCloudSaveState& OnState) = 0;

	virtual void CreateArchive(
		const FTapArchiveMetadata& Metadata,
		const FString& ArchiveFilePath,
		const FString& CoverFilePath,
		const FTapOnArchiveResult& OnSuccess,
		const FTapOnArchiveError&  OnError) = 0;

	virtual void UpdateArchive(
		const FString& Uuid,
		const FTapArchiveMetadata& Metadata,
		const FString& ArchiveFilePath,
		const FString& CoverFilePath,
		const FTapOnArchiveResult& OnSuccess,
		const FTapOnArchiveError&  OnError) = 0;

	virtual void DeleteArchive(
		const FString& Uuid,
		const FTapOnArchiveResult& OnSuccess,
		const FTapOnArchiveError&  OnError) = 0;

	virtual void GetArchiveList(
		const FTapOnArchiveListResult& OnSuccess,
		const FTapOnArchiveError&      OnError) = 0;

	virtual void GetArchiveData(
		const FString& Uuid,
		const FString& FileId,
		const FTapOnArchiveBinaryResult& OnSuccess,
		const FTapOnArchiveError&        OnError) = 0;

	virtual void GetArchiveCover(
		const FString& Uuid,
		const FString& FileId,
		const FTapOnArchiveBinaryResult& OnSuccess,
		const FTapOnArchiveError&        OnError) = 0;
};

class FTapCloudSavePlatformFactory
{
public:
	static TSharedPtr<ITapCloudSavePlatform> CreatePlatform();
#if WITH_DEV_AUTOMATION_TESTS
	static void                              SetPlatformForTest(TSharedPtr<ITapCloudSavePlatform> InTest);
#endif

	// Release the cached platform instance. Called from FTapCloudSaveModule::
	// ShutdownModule so the desktop wrapper's DLL handle is freed on module
	// reload (Editor live coding, plugin disable/enable).
	static void                              Reset();

private:
	static TSharedPtr<ITapCloudSavePlatform> PlatformInstance;
#if WITH_DEV_AUTOMATION_TESTS
	static TSharedPtr<ITapCloudSavePlatform> TestInstance;
#endif
};

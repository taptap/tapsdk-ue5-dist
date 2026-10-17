// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Platform/ITapCloudSavePlatform.h"

/**
 * Test double for ITapCloudSavePlatform used by the TapCloudSave unit tests.
 *
 * Each virtual is implemented synchronously so tests can assert on results
 * without spinning the event loop. Set bForceError = true to make every CRUD
 * entrypoint invoke the OnError delegate with {ErrCode, ErrMsg}; otherwise
 * the matching OnSuccess delegate is invoked with the configured fake fields
 * (FakeArchive / FakeList / FakeBytes).
 *
 * Inject with: FTapCloudSavePlatformFactory::SetPlatformForTest(MakeShared<...>())
 * and reset with: FTapCloudSavePlatformFactory::SetPlatformForTest(nullptr).
 */
class FFakeTapCloudSavePlatform : public ITapCloudSavePlatform
{
public:
	bool bForceError = false;
	int32 ErrCode = 9999;
	FString ErrMsg = TEXT("fake error");

	FTapArchiveData FakeArchive;
	TArray<FTapArchiveData> FakeList;
	TArray<uint8> FakeBytes;

	// Paths as actually received by the platform layer. Tests assert these are
	// byte-identical to what the caller passed, because rewriting them in the
	// common layer breaks the sandbox mapping each mobile platform performs.
	FString LastArchiveFilePath;
	FString LastCoverFilePath;

	virtual void Initialize() override {}
	virtual bool IsInitialized() const override { return true; }
	virtual FString GetPlatformName() const override { return TEXT("Fake"); }

	virtual void RegisterStateCallback(const FTapOnCloudSaveState& /*OnState*/) override {}
	virtual void UnregisterStateCallback(const FTapOnCloudSaveState& /*OnState*/) override {}

	virtual void CreateArchive(const FTapArchiveMetadata& /*Metadata*/, const FString& ArchiveFilePath, const FString& CoverFilePath,
		const FTapOnArchiveResult& OnSuccess, const FTapOnArchiveError& OnError) override
	{
		LastArchiveFilePath = ArchiveFilePath;
		LastCoverFilePath   = CoverFilePath;
		FireArchive(OnSuccess, OnError);
	}

	virtual void UpdateArchive(const FString& /*Uuid*/, const FTapArchiveMetadata& /*Metadata*/, const FString& ArchiveFilePath, const FString& CoverFilePath,
		const FTapOnArchiveResult& OnSuccess, const FTapOnArchiveError& OnError) override
	{
		LastArchiveFilePath = ArchiveFilePath;
		LastCoverFilePath   = CoverFilePath;
		FireArchive(OnSuccess, OnError);
	}

	virtual void DeleteArchive(const FString& /*Uuid*/,
		const FTapOnArchiveResult& OnSuccess, const FTapOnArchiveError& OnError) override
	{
		FireArchive(OnSuccess, OnError);
	}

	virtual void GetArchiveList(const FTapOnArchiveListResult& OnSuccess, const FTapOnArchiveError& OnError) override
	{
		if (bForceError) { OnError.ExecuteIfBound(ErrCode, ErrMsg); }
		else             { OnSuccess.ExecuteIfBound(FakeList); }
	}

	virtual void GetArchiveData(const FString& /*Uuid*/, const FString& /*FileId*/,
		const FTapOnArchiveBinaryResult& OnSuccess, const FTapOnArchiveError& OnError) override
	{
		if (bForceError) { OnError.ExecuteIfBound(ErrCode, ErrMsg); }
		else             { OnSuccess.ExecuteIfBound(FakeBytes); }
	}

	virtual void GetArchiveCover(const FString& /*Uuid*/, const FString& /*FileId*/,
		const FTapOnArchiveBinaryResult& OnSuccess, const FTapOnArchiveError& OnError) override
	{
		if (bForceError) { OnError.ExecuteIfBound(ErrCode, ErrMsg); }
		else             { OnSuccess.ExecuteIfBound(FakeBytes); }
	}

private:
	void FireArchive(const FTapOnArchiveResult& OnSuccess, const FTapOnArchiveError& OnError)
	{
		if (bForceError) { OnError.ExecuteIfBound(ErrCode, ErrMsg); }
		else             { OnSuccess.ExecuteIfBound(FakeArchive); }
	}
};

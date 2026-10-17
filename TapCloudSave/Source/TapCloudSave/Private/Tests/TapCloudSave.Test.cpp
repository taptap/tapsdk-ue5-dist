// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapCloudSave.h"
#include "TapCloudSaveTypes.h"
#include "Internal/TapCloudSaveInternal.h"
#include "Platform/ITapCloudSavePlatform.h"
#include "Internal/TapSdkChecker.h"
#include "Internal/Http/TapHttp.h"
#include "Tests/FakeTapCloudSavePlatform.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	void SetSdkInitializedForFacadeTest(bool bInitialized)
	{
		if (bInitialized && !FTapHttp::IsInitialized())
		{
			FTapHttp::Init(TEXT("test_client"), TEXT("test_token"), ETapRegion::CN, false);
		}
		FTapSdkChecker::SetInitializationState(bInitialized, false);
	}
}

/** Validates facade checks, native error passthrough, and internal file persistence helpers. */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapCloudSaveValidationTest,
	"TapSDK.CloudSave.UTapCloudSave.Validation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapCloudSaveValidationTest::RunTest(const FString& /*Parameters*/)
{
	SetSdkInitializedForFacadeTest(true);
	const TSharedRef<FFakeTapCloudSavePlatform> Fake = MakeShared<FFakeTapCloudSavePlatform>();
	FTapCloudSavePlatformFactory::SetPlatformForTest(Fake);

	// 1) CreateArchive: empty Name -> INVALID_PARAM, message mentions "name"
	{
		int32 GotCode = 0; FString GotMsg;
		UTapCloudSave::CreateArchive(FTapArchiveMetadata(), TEXT("/no.dat"), TEXT(""),
			FTapOnArchiveResult(),
			FTapOnArchiveError::CreateLambda([&](int32 C, const FString& M){ GotCode = C; GotMsg = M; }));
		TestEqual(TEXT("CreateArchive empty name -> INVALID_PARAM"),
			GotCode, TapCloudSaveResultCode::INVALID_PARAM);
		TestTrue(TEXT("CreateArchive empty name -> message mentions 'name'"),
			GotMsg.Contains(TEXT("name")));
	}

	// 2) CreateArchive: name set but archive file not on disk -> INVALID_PARAM, "not found"
	{
		int32 GotCode = 0; FString GotMsg;
		FTapArchiveMetadata Meta; Meta.Name = TEXT("slot1");
		UTapCloudSave::CreateArchive(Meta, TEXT("/definitely/does/not/exist.dat"), TEXT(""),
			FTapOnArchiveResult(),
			FTapOnArchiveError::CreateLambda([&](int32 C, const FString& M){ GotCode = C; GotMsg = M; }));
		TestEqual(TEXT("CreateArchive missing file -> INVALID_PARAM"),
			GotCode, TapCloudSaveResultCode::INVALID_PARAM);
		TestTrue(TEXT("CreateArchive missing file -> message contains 'not found'"),
			GotMsg.Contains(TEXT("not found")));
	}

	// 3) UpdateArchive: native validation owns the empty UUID error.
	{
		const FString ArchivePath = FPaths::CreateTempFilename(
			*FPaths::ProjectSavedDir(), TEXT("cloudsave-validation-"), TEXT(".bin"));
		TestTrue(TEXT("Create UpdateArchive fixture"),
			FFileHelper::SaveStringToFile(TEXT("archive"), *ArchivePath));
		FTapArchiveMetadata Meta;
		Meta.Name = TEXT("slot1");
		Fake->bForceError = true;
		Fake->ErrCode = -1;
		Fake->ErrMsg = TEXT("invalid saveUUID");
		int32 GotCode = 0; FString GotMsg;
		UTapCloudSave::UpdateArchive(TEXT(""), Meta, ArchivePath, TEXT(""),
			FTapOnArchiveResult(),
			FTapOnArchiveError::CreateLambda([&](int32 C, const FString& M){ GotCode = C; GotMsg = M; }));
		TestEqual(TEXT("UpdateArchive preserves native error code"), GotCode, -1);
		TestEqual(TEXT("UpdateArchive preserves native error message"), GotMsg, FString(TEXT("invalid saveUUID")));
		IFileManager::Get().Delete(*ArchivePath);
	}

	// 4) DeleteArchive: native validation owns the empty UUID error.
	{
		Fake->bForceError = true;
		Fake->ErrCode = -1;
		Fake->ErrMsg = TEXT("invalid saveUUID");
		int32 GotCode = 0; FString GotMsg;
		UTapCloudSave::DeleteArchive(TEXT(""),
			FTapOnArchiveResult(),
			FTapOnArchiveError::CreateLambda([&](int32 C, const FString& M){ GotCode = C; GotMsg = M; }));
		TestEqual(TEXT("DeleteArchive preserves native error code"), GotCode, -1);
		TestEqual(TEXT("DeleteArchive preserves native error message"), GotMsg, FString(TEXT("invalid saveUUID")));
	}

	// 5) GetArchiveData: empty FileId is rejected before platform dispatch.
	{
		int32 GotCode = 0; FString GotMsg;
		UTapCloudSave::GetArchiveData(TEXT("uuid_x"), TEXT(""),
			FTapOnArchiveBinaryResult(),
			FTapOnArchiveError::CreateLambda([&](int32 C, const FString& M){ GotCode = C; GotMsg = M; }));
		TestEqual(TEXT("GetArchiveData empty file_id -> -1"), GotCode, -1);
		TestEqual(TEXT("GetArchiveData empty file_id -> required message"), GotMsg, FString(TEXT("Archive file_id required")));
	}

	// Byte downloads have no filesystem prerequisites; internal helpers own demo persistence.
	Fake->bForceError = false;
	Fake->FakeBytes = {0, 1, 255};
	for (bool bCover : {false, true})
	{
		int32 SuccessCount = 0, ErrorCount = 0;
		auto BytesCallback = FTapOnArchiveBinaryResult::CreateLambda([&](const TArray<uint8>& Bytes)
		{
			++SuccessCount;
			TestEqual(TEXT("Bytes are preserved"), Bytes, Fake->FakeBytes);
		});
		auto ErrorCallback = FTapOnArchiveError::CreateLambda([&](int32, const FString&) { ++ErrorCount; });
		if (bCover) UTapCloudSave::GetArchiveCover(TEXT("uuid"), TEXT("file"), BytesCallback, ErrorCallback);
		else UTapCloudSave::GetArchiveData(TEXT("uuid"), TEXT("file"), BytesCallback, ErrorCallback);
		TestEqual(TEXT("One download success"), SuccessCount, 1);
		TestEqual(TEXT("No download error"), ErrorCount, 0);

		const FString Directory = FPaths::CreateTempFilename(*FPaths::ProjectSavedDir(), TEXT("cloudsave-download-"), TEXT(""));
		const FString Path = FPaths::Combine(Directory, TEXT("archive.bin"));
		auto FileCallback = FTapOnArchiveFileResultInternal::CreateLambda([&](const TArray<uint8>& Bytes, const FString& SavedPath)
		{
			++SuccessCount;
			TestEqual(TEXT("Saved path preserved"), SavedPath, Path);
		});
		if (bCover) FTapCloudSaveInternal::GetArchiveCoverToFile(TEXT("uuid"), TEXT("file"), Path, FileCallback, ErrorCallback);
		else FTapCloudSaveInternal::GetArchiveDataToFile(TEXT("uuid"), TEXT("file"), Path, FileCallback, ErrorCallback);
		TArray<uint8> SavedBytes;
		TestTrue(TEXT("Missing directory is created and file saved"), FFileHelper::LoadFileToArray(SavedBytes, *Path));
		TestEqual(TEXT("Saved bytes match download"), SavedBytes, Fake->FakeBytes);
		TestEqual(TEXT("File success fires once"), SuccessCount, 2);

		// A regular file cannot act as the parent of another file.
		const FString InvalidPath = FPaths::Combine(Path, TEXT("child.bin"));
		if (bCover) FTapCloudSaveInternal::GetArchiveCoverToFile(TEXT("uuid"), TEXT("file"), InvalidPath, FileCallback, ErrorCallback);
		else FTapCloudSaveInternal::GetArchiveDataToFile(TEXT("uuid"), TEXT("file"), InvalidPath, FileCallback, ErrorCallback);
		TestEqual(TEXT("Save failure uses error channel"), ErrorCount, 1);
		TestEqual(TEXT("Save failure never reports success"), SuccessCount, 2);
		Fake->bForceError = true;
		if (bCover) UTapCloudSave::GetArchiveCover(TEXT("uuid"), TEXT("file"), BytesCallback, ErrorCallback);
		else UTapCloudSave::GetArchiveData(TEXT("uuid"), TEXT("file"), BytesCallback, ErrorCallback);
		TestEqual(TEXT("Native download failure uses error channel"), ErrorCount, 2);
		Fake->bForceError = false;
		IFileManager::Get().Delete(*Path);
		IFileManager::Get().DeleteDirectory(*Directory);
	}

	// Cleanup — leave the factory in its default state so subsequent tests
	// (e.g. FTapCloudSaveFactoryDispatchTest) see the real platform impl.
	FTapCloudSavePlatformFactory::SetPlatformForTest(nullptr);
	SetSdkInitializedForFacadeTest(false);
	return true;
}

/**
 * Locks in the invariant whose violation caused "Archive file not found" on
 * iOS/Android: the common layer must hand the platform layer the caller's path
 * verbatim.
 *
 * UE paths like "../../../<Project>/Saved/x.bin" are virtual — on mobile only
 * FIOSPlatformFile/FAndroidPlatformFile know where they actually live, and each
 * platform layer resolves them via ConvertToAbsolutePathForExternalAppFor*.
 * Rewriting the path here (as FPaths::ConvertRelativePathToFull used to do —
 * a no-op on mobile, where BaseDir() is empty) defeats that mapping and the
 * native SDK reports the file as missing.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapCloudSavePathPassthroughTest,
	"TapSDK.CloudSave.UTapCloudSave.PathPassthrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapCloudSavePathPassthroughTest::RunTest(const FString& /*Parameters*/)
{
	SetSdkInitializedForFacadeTest(true);
	const TSharedRef<FFakeTapCloudSavePlatform> Fake = MakeShared<FFakeTapCloudSavePlatform>();
	FTapCloudSavePlatformFactory::SetPlatformForTest(Fake);

	// Build the path exactly the way a game would, so it keeps the UE-relative
	// shape that ProjectSavedDir() yields — that is the shape that regressed.
	const FString ArchivePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("cs_passthrough_archive.bin"));
	const FString CoverPath   = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("cs_passthrough_cover.png"));

	// The files must really exist, otherwise the FileExists input check fires
	// first and we never reach the platform dispatch we want to assert on.
	if (!FFileHelper::SaveStringToFile(TEXT("archive"), *ArchivePath) ||
		!FFileHelper::SaveStringToFile(TEXT("cover"), *CoverPath))
	{
		AddError(TEXT("failed to create the temporary archive/cover fixtures"));
		FTapCloudSavePlatformFactory::SetPlatformForTest(nullptr);
		SetSdkInitializedForFacadeTest(false);
		return false;
	}

	FTapArchiveMetadata Meta; Meta.Name = TEXT("slot1");

	UTapCloudSave::CreateArchive(Meta, ArchivePath, CoverPath,
		FTapOnArchiveResult(), FTapOnArchiveError());
	TestEqual(TEXT("CreateArchive forwards the archive path verbatim"),
		Fake->LastArchiveFilePath, ArchivePath);
	TestEqual(TEXT("CreateArchive forwards the cover path verbatim"),
		Fake->LastCoverFilePath, CoverPath);

	Fake->LastArchiveFilePath.Reset();
	Fake->LastCoverFilePath.Reset();

	UTapCloudSave::UpdateArchive(TEXT("uuid_x"), Meta, ArchivePath, CoverPath,
		FTapOnArchiveResult(), FTapOnArchiveError());
	TestEqual(TEXT("UpdateArchive forwards the archive path verbatim"),
		Fake->LastArchiveFilePath, ArchivePath);
	TestEqual(TEXT("UpdateArchive forwards the cover path verbatim"),
		Fake->LastCoverFilePath, CoverPath);

	IFileManager::Get().Delete(*ArchivePath);
	IFileManager::Get().Delete(*CoverPath);
	FTapCloudSavePlatformFactory::SetPlatformForTest(nullptr);
	SetSdkInitializedForFacadeTest(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

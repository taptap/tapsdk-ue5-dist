// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapCloudSave.h"
#include "TapCloudSaveTypes.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

/**
 * Full-CRUD integration test for TapCloudSave.
 *
 * This test exercises the real platform implementation against the live
 * cloudsave backend, so it requires:
 *   - a logged-in TapTap account on the running editor / standalone
 *   - network access to the cloudsave service
 *
 * Because of these requirements it is intentionally excluded from CI by the
 * `-ci-skip-integration` command-line flag (CI runners pass this flag; local
 * runs do not).
 *
 * Sequence: Create -> GetList (verify uuid present) -> Delete (cleanup).
 * Each step uses a synchronous busy-wait (<= 30s) on a `bDone` flag. We rely
 * on AddError (not asserts) inside the async callbacks because the callbacks
 * may fire from non-game threads in some platform impls.
 */
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTapCloudSaveIntegrationTest,
	"TapSDK.CloudSave.Integration.FullCRUD",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::PerfFilter)

void FTapCloudSaveIntegrationTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("FullCRUDRoundTrip"));
	OutTestCommands.Add(TEXT("FullCRUDRoundTrip"));
}

namespace
{
	/** Spin until bDone or until 30s elapses. Returns true if completed in time. */
	bool WaitForDone(const bool& bDone, double TimeoutSeconds = 30.0)
	{
		const double Start = FPlatformTime::Seconds();
		while (!bDone && (FPlatformTime::Seconds() - Start) < TimeoutSeconds)
		{
			FPlatformProcess::Sleep(0.05f);
		}
		return bDone;
	}
}

bool FTapCloudSaveIntegrationTest::RunTest(const FString& Parameters)
{
	// Live backend/account test: run only when explicitly requested. This keeps
	// broad local/CI automation runs deterministic and avoids treating a missing
	// login/session as a product regression.
	if (!FParse::Param(FCommandLine::Get(), TEXT("run-cloudsave-integration")) ||
		FParse::Param(FCommandLine::Get(), TEXT("ci-skip-integration")))
	{
		AddInfo(TEXT("Skipping live CloudSave integration test; pass -run-cloudsave-integration with a logged-in account to enable it."));
		return true;
	}

	// 1) Write a temporary archive payload to ProjectIntermediateDir.
	const FString FilePath = FPaths::Combine(
		FPaths::ProjectIntermediateDir(),
		TEXT("cloudsave_it_save.dat"));
	const FString Payload = TEXT("hello cloud save");
	if (!FFileHelper::SaveStringToFile(Payload, *FilePath))
	{
		AddError(FString::Printf(TEXT("Failed to write temp archive file: %s"), *FilePath));
		return false;
	}

	// 2) Create
	bool bCreateDone = false;
	FTapArchiveData Created;
	FTapArchiveMetadata Meta;
	Meta.Name = TEXT("ITSave1");
	Meta.Summary = TEXT("created by FTapCloudSaveIntegrationTest");
	Meta.Playtime = 1;

	UTapCloudSave::CreateArchive(Meta, FilePath, FString(),
		FTapOnArchiveResult::CreateLambda([&Created, &bCreateDone](const FTapArchiveData& Data)
		{
			Created = Data;
			bCreateDone = true;
		}),
		FTapOnArchiveError::CreateLambda([this, &bCreateDone](int32 Code, const FString& Msg)
		{
			AddError(FString::Printf(TEXT("Create failed: %d %s"), Code, *Msg));
			bCreateDone = true;
		}));

	if (!WaitForDone(bCreateDone))
	{
		AddError(TEXT("Create did not complete within 30s"));
		return false;
	}
	if (Created.Uuid.IsEmpty())
	{
		AddError(TEXT("Create returned but uuid is empty; aborting list/delete steps"));
		return false;
	}

	// 3) GetList (verify uuid present)
	bool bListDone = false;
	UTapCloudSave::GetArchiveList(
		FTapOnArchiveListResult::CreateLambda([this, &Created, &bListDone](const TArray<FTapArchiveData>& List)
		{
			const bool bFound = List.ContainsByPredicate([&Created](const FTapArchiveData& E)
			{
				return E.Uuid == Created.Uuid;
			});
			if (!bFound)
			{
				AddError(FString::Printf(TEXT("List did not contain uuid=%s (size=%d)"),
					*Created.Uuid, List.Num()));
			}
			bListDone = true;
		}),
		FTapOnArchiveError::CreateLambda([this, &bListDone](int32 Code, const FString& Msg)
		{
			AddError(FString::Printf(TEXT("List failed: %d %s"), Code, *Msg));
			bListDone = true;
		}));

	if (!WaitForDone(bListDone))
	{
		AddError(TEXT("List did not complete within 30s"));
		// Still try to clean up.
	}

	// 4) Delete (cleanup)
	bool bDeleteDone = false;
	UTapCloudSave::DeleteArchive(Created.Uuid,
		FTapOnArchiveResult::CreateLambda([&bDeleteDone](const FTapArchiveData& /*D*/)
		{
			bDeleteDone = true;
		}),
		FTapOnArchiveError::CreateLambda([this, &bDeleteDone](int32 Code, const FString& Msg)
		{
			AddError(FString::Printf(TEXT("Delete failed: %d %s"), Code, *Msg));
			bDeleteDone = true;
		}));

	if (!WaitForDone(bDeleteDone))
	{
		AddError(TEXT("Delete did not complete within 30s"));
		return false;
	}

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

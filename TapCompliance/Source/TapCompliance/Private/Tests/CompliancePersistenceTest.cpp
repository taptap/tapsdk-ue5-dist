// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "CompliancePersistence.h"
#include "ComplianceState.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/Storage/TapSharedCacheFile.h"
#include "Containers/Ticker.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	struct FScopedPersistenceFixture
	{
		FString OldClientId = FComplianceState::Get().ClientId;
		FString RelativeDirectory;
		FString Directory;

		FScopedPersistenceFixture()
		{
			FComplianceState::Get().ClientId = TEXT("persistence_test_") + FGuid::NewGuid().ToString();
			RelativeDirectory = FPaths::Combine(TEXT("TapCompliance"), FComplianceState::Get().ClientId);
			Directory = FPaths::Combine(FPaths::ProjectSavedDir(), RelativeDirectory);
		}

		~FScopedPersistenceFixture()
		{
			FComplianceState::Get().ClientId = OldClientId;
			IFileManager::Get().DeleteDirectory(*Directory, false, true);
		}

		FString UserFile(const FString& UserId) const
		{
			return FPaths::Combine(Directory, TEXT("users"), FMD5::HashAnsiString(*UserId) + TEXT(".json"));
		}
	};

	FComplianceVerification Verification(const FString& UserId, const FString& Token)
	{
		FComplianceVerification Result;
		Result.UserId = UserId;
		Result.ComplianceToken = Token;
		Result.Status = TEXT("pass");
		return Result;
	}

	FString Json(const TSharedPtr<FJsonObject>& Object)
	{
		FString Result;
		FJsonSerializer::Serialize(Object.ToSharedRef(), TJsonWriterFactory<>::Create(&Result));
		return Result;
	}

	FString ReadFile(const FString& Path)
	{
		FString Result;
		FFileHelper::LoadFileToString(Result, *Path);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePersistenceUserFilesTest,
	"TapCompliance.Persistence.UserFiles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePersistenceUserFilesTest::RunTest(const FString&)
{
	FScopedPersistenceFixture Fixture;
	const FString UserA = TEXT("玩家甲/../A");
	const FString UserB = TEXT("玩家乙/../B");
	TestTrue(TEXT("Save A"), FCompliancePersistence::SaveVerification(UserA, Verification(UserA, TEXT("token-a"))));
	TestTrue(TEXT("Save B"), FCompliancePersistence::SaveVerification(UserB, Verification(UserB, TEXT("token-b"))));
	FUserAntiConfig Config;
	Config.UserState.AgeLimit = 16;
	FCompliancePersistence::SaveUserAntiConfig(UserB, Config);
	FCompliancePersistence::MarkFirstPassTipShown(UserB);
	const FString BBefore = ReadFile(Fixture.UserFile(UserB));
	TestFalse(TEXT("B file exists"), BBefore.IsEmpty());
	TestTrue(TEXT("User filenames differ"), Fixture.UserFile(UserA) != Fixture.UserFile(UserB));
	TestFalse(TEXT("B file does not contain A's token"), BBefore.Contains(TEXT("token-a")));

	TestTrue(TEXT("Logout A removes A verification"), FCompliancePersistence::DeleteVerification(UserA));
	TestEqual(TEXT("Logout A does not write B's file"), ReadFile(Fixture.UserFile(UserB)), BBefore);
	TestTrue(TEXT("Simulate interrupted write of A"), FFileHelper::SaveStringToFile(TEXT("{"), *Fixture.UserFile(UserA)));
	FComplianceVerification Loaded;
	TestFalse(TEXT("Corrupt A is a cache miss"), FCompliancePersistence::LoadVerification(UserA, Loaded));
	TestTrue(TEXT("B still loads after A is corrupt"), FCompliancePersistence::LoadVerification(UserB, Loaded));
	TestEqual(TEXT("B token preserved"), Loaded.ComplianceToken, FString(TEXT("token-b")));
	FUserAntiConfig LoadedConfig;
	TestTrue(TEXT("B config preserved"), FCompliancePersistence::LoadUserAntiConfig(UserB, LoadedConfig));
	TestEqual(TEXT("B age preserved"), LoadedConfig.UserState.AgeLimit, 16);
	TestTrue(TEXT("B tip preserved"), FCompliancePersistence::HasShownFirstPassTip(UserB));
	TestFalse(TEXT("A does not inherit B's tip"), FCompliancePersistence::HasShownFirstPassTip(UserA));

	FRealNameConfigResult Global;
	Global.bUseManual = true;
	TestTrue(TEXT("Save global config"), FCompliancePersistence::SaveRealNameConfig(Global));
	TestEqual(TEXT("Global save does not touch B"), ReadFile(Fixture.UserFile(UserB)), BBefore);
	TestTrue(TEXT("Global file is separate"), IFileManager::Get().FileExists(*FPaths::Combine(Fixture.Directory, TEXT("config.json"))));
	TestFalse(TEXT("Fresh installation does not create legacy cache"), IFileManager::Get().FileExists(*FPaths::Combine(Fixture.Directory, TEXT("cache.json"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePersistenceMigrationTest,
	"TapCompliance.Persistence.LegacyMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePersistenceMigrationTest::RunTest(const FString&)
{
	FScopedPersistenceFixture Fixture;
	const FString UserA = TEXT("user-a");
	const FString UserB = TEXT("user-b");
	const FString HashA = FMD5::HashAnsiString(*UserA);
	const FString HashB = FMD5::HashAnsiString(*UserB);
	FTapMachineBoundPrefs Legacy;
	Legacy.Init(FPaths::Combine(Fixture.RelativeDirectory, TEXT("cache.json")));
	Legacy.Set(TEXT("verification:") + HashA, Json(Verification(UserA, TEXT("legacy-a")).ToJson()));
	Legacy.Set(TEXT("verification:") + HashB, Json(Verification(UserB, TEXT("legacy-b")).ToJson()));
	FUserAntiConfig Config;
	Config.UserState.AgeLimit = 16;
	Legacy.Set(TEXT("user_config:") + HashA, Json(Config.ToJson()));
	Legacy.Set(TEXT("first_pass_tip:") + HashA, TEXT("1"));
	FRealNameConfigResult Global;
	Global.bUseManual = true;
	Legacy.Set(TEXT("realname_config"), Json(Global.ToJson()));
	Legacy.Save();
	const FString LegacyPath = FPaths::Combine(Fixture.Directory, TEXT("cache.json"));
	const FString Before = ReadFile(LegacyPath);

	FComplianceVerification Loaded;
	auto LegacyLease = FTapSharedCacheFile::TryAcquire(LegacyPath);
	TestFalse(TEXT("Busy legacy is a cache miss"), FCompliancePersistence::LoadVerification(UserA, Loaded));
	TestFalse(TEXT("Busy legacy does not create a false migration marker"), IFileManager::Get().FileExists(*Fixture.UserFile(UserA)));
	LegacyLease.Reset();
	TestTrue(TEXT("Simulate malformed legacy"), FFileHelper::SaveStringToFile(TEXT("{"), *LegacyPath));
	TestFalse(TEXT("Corrupt legacy is a cache miss"), FCompliancePersistence::LoadVerification(UserA, Loaded));
	TestEqual(TEXT("Corrupt legacy is never rebuilt by Prefs"), ReadFile(LegacyPath), FString(TEXT("{")));
	TestFalse(TEXT("Corrupt legacy does not block a later migration"), IFileManager::Get().FileExists(*Fixture.UserFile(UserA)));
	FFileHelper::SaveStringToFile(Before, *LegacyPath);
	TestTrue(TEXT("Migrate A"), FCompliancePersistence::LoadVerification(UserA, Loaded));
	TestEqual(TEXT("A token migrated"), Loaded.ComplianceToken, FString(TEXT("legacy-a")));
	TestFalse(TEXT("A file excludes B"), ReadFile(Fixture.UserFile(UserA)).Contains(TEXT("legacy-b")));
	FUserAntiConfig LoadedConfig;
	TestTrue(TEXT("User config migrated"), FCompliancePersistence::LoadUserAntiConfig(UserA, LoadedConfig));
	TestEqual(TEXT("Age migrated"), LoadedConfig.UserState.AgeLimit, 16);
	TestTrue(TEXT("Tip migrated"), FCompliancePersistence::HasShownFirstPassTip(UserA));
	FRealNameConfigResult LoadedGlobal;
	TestTrue(TEXT("Global config migrated"), FCompliancePersistence::LoadRealNameConfig(LoadedGlobal));
	TestTrue(TEXT("Global value migrated"), LoadedGlobal.bUseManual);
	TestFalse(TEXT("Global file excludes user tokens"), ReadFile(FPaths::Combine(Fixture.Directory, TEXT("config.json"))).Contains(TEXT("legacy-a")));

	TestTrue(TEXT("Delete migrated verification"), FCompliancePersistence::DeleteVerification(UserA));
	TestTrue(TEXT("Delete migrated config"), FCompliancePersistence::DeleteUserAntiConfig(UserA));
	TestFalse(TEXT("Deleted token is not imported again"), FCompliancePersistence::LoadVerification(UserA, Loaded));
	TestFalse(TEXT("Deleted config is not imported again"), FCompliancePersistence::LoadUserAntiConfig(UserA, LoadedConfig));
	TestTrue(TEXT("Tip survives user logout"), FCompliancePersistence::HasShownFirstPassTip(UserA));
	TestTrue(TEXT("B can still migrate afterwards"), FCompliancePersistence::LoadVerification(UserB, Loaded));
	TestEqual(TEXT("B token migrated"), Loaded.ComplianceToken, FString(TEXT("legacy-b")));
	FCompliancePersistence::SaveVerification(UserB, Verification(UserB, TEXT("new-b")));
	FCompliancePersistence::LoadVerification(UserB, Loaded);
	TestEqual(TEXT("New token wins over legacy"), Loaded.ComplianceToken, FString(TEXT("new-b")));
	TestEqual(TEXT("Migration and logout never modify legacy file"), ReadFile(LegacyPath), Before);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePersistenceMachineBindingTest,
	"TapCompliance.Persistence.MachineBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePersistenceMachineBindingTest::RunTest(const FString&)
{
	FScopedPersistenceFixture Fixture;
	const FString User = TEXT("user");
	const FString Key = TEXT("verification:") + FMD5::HashAnsiString(*User);
	FTapMachineBoundPrefs Legacy;
	Legacy.Init(FPaths::Combine(Fixture.RelativeDirectory, TEXT("cache.json")));
	const FString LocalFingerprint = Legacy.GetString(TEXT("tap_machine_fp"));
	Legacy.Set(Key, Json(Verification(User, TEXT("legacy-token")).ToJson()));
	Legacy.Save();
	const FString LegacyPath = FPaths::Combine(Fixture.Directory, TEXT("cache.json"));
	FTapPrefs RawLegacy;
	RawLegacy.Init(FPaths::Combine(Fixture.RelativeDirectory, TEXT("cache.json")));
	RawLegacy.Set(TEXT("tap_machine_fp"), TEXT("foreign-machine"));
	RawLegacy.Save();
	const FString ForeignLegacy = ReadFile(LegacyPath);
	FComplianceVerification Loaded;
	TestFalse(TEXT("Foreign legacy must not migrate"), FCompliancePersistence::LoadVerification(User, Loaded));
	TestEqual(TEXT("Foreign legacy is read-only"), ReadFile(LegacyPath), ForeignLegacy);

	// Even a valid legacy source must not resurrect an existing invalidated/corrupt user file.
	RawLegacy.Set(TEXT("tap_machine_fp"), LocalFingerprint);
	RawLegacy.Save();
	FCompliancePersistence::SaveVerification(User, Verification(User, TEXT("new-token")));
	FTapPrefs RawUser;
	RawUser.Init(FPaths::Combine(Fixture.RelativeDirectory, TEXT("users"), FMD5::HashAnsiString(*User) + TEXT(".json")));
	RawUser.Set(TEXT("tap_machine_fp"), TEXT("foreign-machine"));
	RawUser.Save();
	TestFalse(TEXT("Foreign user cache invalidates without legacy fallback"), FCompliancePersistence::LoadVerification(User, Loaded));
	TestFalse(TEXT("Cleared file stays empty on subsequent read"), FCompliancePersistence::LoadVerification(User, Loaded));
	TestTrue(TEXT("Write corrupt user cache"), FFileHelper::SaveStringToFile(TEXT("{"), *Fixture.UserFile(User)));
	TestFalse(TEXT("Corrupt existing file does not import legacy"), FCompliancePersistence::LoadVerification(User, Loaded));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePersistenceBusyTest,
	"TapCompliance.Persistence.BusySaveAndLogout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePersistenceBusyTest::RunTest(const FString&)
{
	FScopedPersistenceFixture Fixture;
	const FString User = TEXT("busy-user");
	const FString Path = Fixture.UserFile(User);
	const FString Key = TEXT("verification:") + FMD5::HashAnsiString(*User);
	TestTrue(TEXT("Seed old token"), FCompliancePersistence::SaveVerification(User, Verification(User, TEXT("old-token"))));
	const FString Original = ReadFile(Path);
	auto Lease = FTapSharedCacheFile::TryAcquire(Path);
	TestTrue(TEXT("Hold account cache"), Lease.IsValid());
	TestFalse(TEXT("Busy save is not reported as persisted"), FCompliancePersistence::SaveVerification(User, Verification(User, TEXT("new-token"))));
	TestEqual(TEXT("Busy save preserves disk"), ReadFile(Path), Original);
	Lease.Reset();
	FTSTicker::GetCoreTicker().Tick(1.1f);
	FComplianceVerification Loaded;
	TestTrue(TEXT("Deferred save survives API return"), FCompliancePersistence::LoadVerification(User, Loaded));
	TestEqual(TEXT("New token recovered"), Loaded.ComplianceToken, FString(TEXT("new-token")));

	Lease = FTapSharedCacheFile::TryAcquire(Path);
	TestTrue(TEXT("Logout persists invalidation even before prefs finish loading"), FCompliancePersistence::DeleteVerification(User));
	TestTrue(TEXT("Durable logout marker"), IFileManager::Get().FileExists(*(Path + TEXT(".invalid.") + FMD5::HashAnsiString(*Key))));
	TestFalse(TEXT("Busy old token is no longer usable"), FCompliancePersistence::LoadVerification(User, Loaded));
	Lease.Reset();
	FTSTicker::GetCoreTicker().Tick(1.1f);
	TestFalse(TEXT("Deferred removal completed"), FCompliancePersistence::LoadVerification(User, Loaded));
	TestFalse(TEXT("Deleted token removed from disk"), ReadFile(Path).Contains(TEXT("new-token")));
	TestTrue(TEXT("Simulate stale data surviving a crash"), FFileHelper::SaveStringToFile(Original, *Path));
	TestFalse(TEXT("Persistent marker prevents restoring stale token"), FCompliancePersistence::LoadVerification(User, Loaded));
	TestTrue(TEXT("New authoritative save clears invalidation"), FCompliancePersistence::SaveVerification(User, Verification(User, TEXT("fresh-token"))));
	TestTrue(TEXT("Fresh token available"), FCompliancePersistence::LoadVerification(User, Loaded));
	TestEqual(TEXT("Fresh token value"), Loaded.ComplianceToken, FString(TEXT("fresh-token")));
	return true;
}

#endif

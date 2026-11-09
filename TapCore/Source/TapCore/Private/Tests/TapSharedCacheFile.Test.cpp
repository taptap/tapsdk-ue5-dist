// Copyright TapTap. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)
#include "Internal/Storage/TapSharedCacheFile.h"
#include "Internal/Storage/TapPrefs.h"
#include "Internal/TapDBEventSender.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"

namespace
{
	FString CacheTestPath()
	{
		return TEXT("Automation/SharedCache-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("/cache.json");
	}
	FString ReadCache(const FString& Path)
	{
		FString Result;
		FFileHelper::LoadFileToString(Result, *Path);
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapSharedFileWriteTest, "TapSDK.SharedCache.AtomicWriteAndBusy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapSharedFileWriteTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), CacheTestPath());
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true); };
	TestTrue(TEXT("Write old cache"), FTapSharedCacheFile::Write(Path, TEXT("old")));
	auto Lease = FTapSharedCacheFile::TryAcquire(Path);
	TestTrue(TEXT("Acquire lease"), !!Lease);
	const double Start = FPlatformTime::Seconds();
	TestFalse(TEXT("Busy write returns without overwriting"), FTapSharedCacheFile::Write(Path, TEXT("new")));
	TestTrue(TEXT("Busy write does not wait seconds"), FPlatformTime::Seconds() - Start < 0.5);
	TestEqual(TEXT("Old content intact"), ReadCache(Path), FString(TEXT("old")));
	Lease.Reset();
	TestTrue(TEXT("Write succeeds after release"), FTapSharedCacheFile::Write(Path, TEXT("new")));
	TestEqual(TEXT("Published content readable"), ReadCache(Path), FString(TEXT("new")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapSharedPrefsMergeTest, "TapSDK.SharedCache.PrefsDirtyMerge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapSharedPrefsMergeTest::RunTest(const FString&)
{
	const FString Relative = CacheTestPath();
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), Relative);
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true); };
	FTapPrefs A, B;
	A.Init(Relative);
	B.Init(Relative); // Two snapshots taken before either process writes.
	A.Set(TEXT("a"), TEXT("a1")); A.Save();
	B.Set(TEXT("b"), TEXT("b1")); B.Save();
	FTapPrefs Read;
	Read.Init(Relative);
	TestEqual(TEXT("B did not erase A's key"), Read.GetString(TEXT("a")), FString(TEXT("a1")));
	TestEqual(TEXT("B persisted its key"), Read.GetString(TEXT("b")), FString(TEXT("b1")));
	A.Remove(TEXT("a"));
	auto Lease = FTapSharedCacheFile::TryAcquire(Path);
	A.Set(TEXT("c"), TEXT("pending")); A.Save();
	Lease.Reset();
	B.Set(TEXT("b"), TEXT("b2")); B.Save();
	A.Save();
	Read.Init(Relative);
	TestTrue(TEXT("Pending removal retried"), Read.GetString(TEXT("a")).IsEmpty());
	TestEqual(TEXT("Pending value retried"), Read.GetString(TEXT("c")), FString(TEXT("pending")));
	TestEqual(TEXT("Retry merges latest disk values"), Read.GetString(TEXT("b")), FString(TEXT("b2")));
	A.Clear(); A.Set(TEXT("after-clear"), TEXT("yes")); A.Save();
	Read.Init(Relative);
	TestTrue(TEXT("Explicit clear still removes previous values"), Read.GetString(TEXT("b")).IsEmpty());
	TestEqual(TEXT("Set after clear remains persistent"), Read.GetString(TEXT("after-clear")), FString(TEXT("yes")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapSharedEventCacheTest, "TapSDK.SharedCache.EventPersistenceRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapSharedEventCacheTest::RunTest(const FString&)
{
	const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), CacheTestPath());
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true); };
	FTapDBEventSender Sender;
	Sender.EventCacheFilePath = Path;
	Sender.bCacheLoaded = Sender.LoadEvents();
	TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("name"), TEXT("pending-event"));
	Sender.EventQueue.Add(Event);
	Sender.SaveEvents();
	TestTrue(TEXT("Uncontended send persists immediately"), ReadCache(Path).Contains(TEXT("pending-event")));
	auto Lease = FTapSharedCacheFile::TryAcquire(Path);
	Sender.EventQueue.Empty();
	Sender.SaveEvents();
	TestTrue(TEXT("Busy cleanup remains pending"), Sender.bSavePending);
	TestTrue(TEXT("Busy write preserves old file"), ReadCache(Path).Contains(TEXT("pending-event")));
	Lease.Reset();
	Sender.bIsInitialized = true;
	Sender.Flush();
	TestFalse(TEXT("Empty Flush retries persistence"), Sender.bSavePending);
	TestFalse(TEXT("Cleanup finishes"), ReadCache(Path).Contains(TEXT("pending-event")));
	Sender.Shutdown();
	FTapSharedCacheFile::Write(Path, TEXT("{"));
	FTapDBEventSender Corrupt;
	Corrupt.EventCacheFilePath = Path;
	Corrupt.bCacheLoaded = Corrupt.LoadEvents();
	Corrupt.EventQueue.Add(Event);
	Corrupt.SaveEvents();
	TestEqual(TEXT("Failed load cannot overwrite the old queue"), ReadCache(Path), FString(TEXT("{")));
	FTapSharedCacheFile::Write(Path, TEXT("{\"data\":[]}"));
	Corrupt.SaveEvents();
	TestFalse(TEXT("Save recovers after readable cache returns"), Corrupt.bSavePending);
	TestTrue(TEXT("New event retained through read failure"), ReadCache(Path).Contains(TEXT("pending-event")));
	return true;
}
#endif

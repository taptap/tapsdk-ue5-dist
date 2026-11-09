// Copyright TapTap. All Rights Reserved.
#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC)
#include "Internal/Storage/TapPrefs.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/Storage/TapSharedCacheFile.h"
#include "Internal/TapDBEventSender.h"
#include "Internal/TapDBUserManager.h"
#include "Internal/TapDBEventTracker.h"
#include "Storage/TapEventStorage.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

namespace TapStorageTests
{
FString UniquePath() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), FGuid::NewGuid().ToString(EGuidFormats::Digits), TEXT("prefs.json"))); }
FString Read(const FString& Path) { FString Json; FFileHelper::LoadFileToString(Json, *Path); return Json; }
bool Wait(TFunction<bool()> Predicate)
{
	const double End = FPlatformTime::Seconds() + 4;
	do { if (Predicate()) return true; FPlatformProcess::Sleep(0.02f); } while (FPlatformTime::Seconds() < End);
	return false;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapPrefsAutomaticRetryTest, "TapSDK.SharedCache.AutomaticRetryAndReadiness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapPrefsAutomaticRetryTest::RunTest(const FString&)
{
	const FString Path = TapStorageTests::UniquePath();
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true); };
	FTapSharedCacheFile::Write(Path, TEXT("{\"old\":\"value\",\"edit\":\"old\",\"delete_before_load\":\"stale\"}"));
	auto Lease = FTapSharedCacheFile::TryAcquire(Path);
	FTapPrefs Prefs;
	Prefs.Init(Path);
	TestFalse(TEXT("Lock contention delays readiness"), Prefs.IsReady());
	Prefs.Remove(TEXT("delete_before_load"));
	Prefs.Set(TEXT("edit"), TEXT("new")); Prefs.Save();
	Lease.Reset();
	TestTrue(TEXT("Ready and saved without another foreground call"), TapStorageTests::Wait([&]
	{ return Prefs.IsReady() && TapStorageTests::Read(Path).Contains(TEXT("new")); }));
	TestEqual(TEXT("Delayed load retains untouched state"), Prefs.GetString(TEXT("old")), FString(TEXT("value")));
	TestEqual(TEXT("Delayed load preserves foreground edit"), Prefs.GetString(TEXT("edit")), FString(TEXT("new")));
	TestFalse(TEXT("Deletion before loading cannot resurrect disk value"), TapStorageTests::Read(Path).Contains(TEXT("delete_before_load")));
	Lease = FTapSharedCacheFile::TryAcquire(Path);
	Prefs.Remove(TEXT("old")); Prefs.Set(TEXT("retry"), TEXT("saved")); Prefs.Save();
	TestTrue(TEXT("Failed write leaves old data"), TapStorageTests::Read(Path).Contains(TEXT("value")));
	Lease.Reset();
	TestTrue(TEXT("Failed removal retries automatically"), TapStorageTests::Wait([&]
	{ const FString Text = TapStorageTests::Read(Path); return Text.Contains(TEXT("saved")) && !Text.Contains(TEXT("value")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapPrivatePrefsRetryTest, "TapSDK.SharedCache.PrivateAtomicRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapPrivatePrefsRetryTest::RunTest(const FString&)
{
	const FString Path = TapStorageTests::UniquePath();
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true); };
	FTapPrefs Prefs;
	Prefs.Init(Path, true);
	Prefs.Set(TEXT("value"), TEXT("old")); Prefs.Save();
	TestFalse(TEXT("Private file has no sidecar lock"), IFileManager::Get().FileExists(*(Path + TEXT(".lock"))));
	IFileManager::Get().Delete(*Path);
	IFileManager::Get().MakeDirectory(*Path);
	Prefs.Set(TEXT("value"), TEXT("new")); Prefs.Save();
	IFileManager::Get().DeleteDirectory(*Path);
	TestTrue(TEXT("Private write retries without another save"), TapStorageTests::Wait([&] { return TapStorageTests::Read(Path).Contains(TEXT("new")); }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapEventInFlightCacheTest, "TapSDK.SharedCache.InFlightAndAcknowledgement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapEventInFlightCacheTest::RunTest(const FString&)
{
	const FString Path = TapStorageTests::UniquePath();
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true); };
	FTapDBEventSender Sender;
	Sender.EventCacheFilePath = Path;
	Sender.bCacheLoaded = Sender.LoadEvents();
	auto Event = MakeShared<FJsonObject>(); Event->SetStringField(TEXT("name"), TEXT("in-flight"));
	Sender.InFlightBatches.Add(1, {Event});
	Sender.SaveEvents();
	TestTrue(TEXT("In-flight event remains recoverable on disk"), TapStorageTests::Read(Path).Contains(TEXT("in-flight")));
	auto Lease = FTapSharedCacheFile::TryAcquire(Path);
	Sender.InFlightBatches.Remove(1);
	Sender.OnHttpResponseReceived(true, 200, {Event}, 0);
	TestTrue(TEXT("Acknowledged cleanup remains pending when busy"), Sender.bSavePending);
	Lease.Reset();
	Sender.bIsInitialized = true;
	Sender.Flush();
	TestFalse(TEXT("Empty queue retries acknowledged cleanup"), TapStorageTests::Read(Path).Contains(TEXT("in-flight")));
	Sender.OnHttpResponseReceived(false, 500, {Event}, TapDBConstants::MAX_RETRY);
	TestEqual(TEXT("Exhausted retry returns the batch to the pending queue"), Sender.EventQueue.Num(), 1);
	TestTrue(TEXT("Failed batch remains persisted for the next trigger"), TapStorageTests::Read(Path).Contains(TEXT("in-flight")));
	Sender.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapIsolatedDurationTest, "TapSDK.SharedCache.IsolatedDurationAndReentry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapIsolatedDurationTest::RunTest(const FString&)
{
	const FString Path = TapStorageTests::UniquePath();
	ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*FPaths::GetPath(Path), false, true); };
	FTapMachineBoundPrefs Prefs; Prefs.Init(Path, true);
	FTapDBUserManager User; User.Init(Prefs, true);
	TArray<TSharedPtr<FJsonObject>> Sent;
	User.SendDurationSnapshot = [&](TSharedPtr<FJsonObject> Event) { Sent.Add(Event); };
	User.BuildDurationSnapshot = [](const FString& Id, int64 Duration)
	{
		auto Event = MakeShared<FJsonObject>();
		Event->SetStringField(TEXT("client_id"), TEXT("client"));
		Event->SetStringField(TEXT("user_id"), Id);
		Event->SetStringField(TEXT("name"), TEXT("play_game"));
		auto Props = MakeShared<FJsonObject>(); Props->SetNumberField(TEXT("duration"), Duration);
		Event->SetObjectField(TEXT("properties"), Props); return Event;
	};
	User.Login(TEXT("a")); User.OnPlayTimerTick(2); User.OnPlayTimerTick(2);
	User.Login(TEXT("b"));
	TestEqual(TEXT("Switch reports exactly the old account snapshot"), Sent.Num(), 1);
	if (Sent.Num() == 1)
	{
		TestEqual(TEXT("Account retained"), Sent[0]->GetStringField(TEXT("user_id")), FString(TEXT("a")));
		TestEqual(TEXT("Duration retained"), Sent[0]->GetObjectField(TEXT("properties"))->GetIntegerField(TEXT("duration")), 4);
	}
	User.OnPlayTimerTick(2);
	User.BuildDurationSnapshot = [&](const FString&, int64)
	{
		User.Login(TEXT("c"));
		return MakeShared<FJsonObject>();
	};
	TestFalse(TEXT("Reentrant change cancels the old recording tick"), User.OnPlayTimerTick(2));
	TestTrue(TEXT("Old tick cannot write a snapshot into the new account"), Prefs.GetString(FTapEventStorage::SnapshotKey).IsEmpty());
	TestTrue(TEXT("No legacy shared duration key"), Prefs.GetString(TapDBConstants::PREF_PLAYED_DURATION + TEXT("_a")).IsEmpty());
	User.Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapDeferredInitializationTest, "TapSDK.SharedCache.DeferredCallsAndCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapDeferredInitializationTest::RunTest(const FString&)
{
	FTapDBEventTracker Tracker;
	Tracker.PendingInitialization = [] { return false; };
	Tracker.SetUserID(TEXT("a"));
	Tracker.ClearUser();
	Tracker.SetUserID(TEXT("b"));
	Tracker.AddCommon(TEXT("{\"level\":7}"));
	TestFalse(TEXT("Pending calls do not run before storage readiness"), Tracker.UserManager.HasUser());
	Tracker.bIsInitialized = true;
	Tracker.DrainPendingCalls();
	TestEqual(TEXT("Account operations replay in order"), Tracker.UserManager.GetCurrentUserID(), FString(TEXT("b")));
	TestTrue(TEXT("Pending common property preserves its numeric type"), Tracker.CommonProperties.Contains(TEXT("level")) && Tracker.CommonProperties[TEXT("level")]->Type == EJson::Number);
	Tracker.Shutdown();
	Tracker.PendingInitialization = [] { return false; };
	bool bCalledAfterShutdown = false;
	Tracker.DeferUntilReady([&] { Tracker.Shutdown(); });
	Tracker.DeferUntilReady([&] { bCalledAfterShutdown = true; });
	Tracker.bIsInitialized = true;
	Tracker.DrainPendingCalls();
	TestFalse(TEXT("Reentrant shutdown cancels remaining queued calls"), bCalledAfterShutdown);
	Tracker.bIsInitialized = true;
	Tracker.RegisterDynamicProperties([&] { Tracker.Shutdown(); return FString(TEXT("{}")); });
	Tracker.LogEvent(TEXT("reentrant"), TEXT(""));
	TestFalse(TEXT("Dynamic callback may shut down its owner safely"), Tracker.IsInitialized());
	return true;
}
#endif

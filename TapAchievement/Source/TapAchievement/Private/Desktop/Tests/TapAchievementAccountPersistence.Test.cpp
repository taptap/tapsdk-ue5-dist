// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC)
#include "Desktop/TapAchievementDesktop.h"
#include "Internal/Storage/TapSharedCacheFile.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementAccountPersistenceTest,
    "TapSDK.Achievement.PendingQueue.AccountPersistence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementAccountPersistenceTest::RunTest(const FString&)
{
    const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        TEXT("achievement-accounts-") + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    ON_SCOPE_EXIT { IFileManager::Get().DeleteDirectory(*Root, false, true); };

    auto Configure = [&](FTapAchievementDesktop& Desktop, const FString& Scenario, const FString& Client = TEXT("game"))
    {
        Desktop.QueueRootForTest = FPaths::Combine(Root, Scenario);
        Desktop.ClientId = Client;
    };
    auto MakeItem = [](const FString& User, const FString& UUID, int32 Steps)
    {
        FTapAchievementPendingItem Item;
        Item.UserId = User;
        Item.UUID = UUID;
        Item.AchievementId = TEXT("steps");
        Item.Steps = Steps;
        return Item;
    };
    auto StepsFor = [](FTapAchievementDesktop& Desktop, const FString& User)
    {
        int32 Total = 0;
        for (const auto& Item : Desktop.PendingQueue.GetItems())
            if (Item.UserId == User) Total += Item.Steps;
        return Total;
    };
    const auto A10 = MakeItem(TEXT("a"), TEXT("a10"), 10);
    const auto B1 = MakeItem(TEXT("b"), TEXT("b1"), 1);

    // Independent instances model separately loaded process memory. Exercise both write orders.
    for (bool bReverse : {false, true})
    {
        const FString Scenario = bReverse ? TEXT("ba") : TEXT("ab");
        FTapAchievementDesktop A, B;
        Configure(A, Scenario);
        Configure(B, Scenario);
        TestTrue(TEXT("A initially loads an empty queue"), A.LoadQueueFromDisk(TEXT("a")));
        TestTrue(TEXT("B initially loads an empty queue"), B.LoadQueueFromDisk(TEXT("b")));
        TestNotEqual(TEXT("Accounts use separate files"), A.GetQueueFilePath(TEXT("a")), B.GetQueueFilePath(TEXT("b")));
        if (bReverse) { B.EnqueuePending(B1); A.EnqueuePending(A10); }
        else { A.EnqueuePending(A10); B.EnqueuePending(B1); }
        TestFalse(TEXT("A's operation is durable before simulated process loss"), A.bQueueNeedsSave);
        TestFalse(TEXT("B's operation is durable before simulated process loss"), B.bQueueNeedsSave);

        // A fresh instance restores disk only, without any state from the original instances.
        FTapAchievementDesktop Restarted;
        Configure(Restarted, Scenario);
        TestTrue(TEXT("Restart loads A"), Restarted.LoadQueueFromDisk(TEXT("a")));
        TestEqual(TEXT("A recovers the original ten steps"), StepsFor(Restarted, TEXT("a")), 10);
        TestEqual(TEXT("A can replay the original operation"), Restarted.PendingQueue.BeginReplay(TEXT("a")).Num(), 1);
        Restarted.EnqueuePending(MakeItem(TEXT("a"), TEXT("new-a"), 2));
        Restarted.FinishPendingReplay(A10.UUID, A10.UserId, true);
        TestEqual(TEXT("Successful replay removes old steps but preserves the new operation"), StepsFor(Restarted, TEXT("a")), 2);
        TestTrue(TEXT("B remains readable after A completes"), Restarted.LoadQueueFromDisk(TEXT("b")));
        TestEqual(TEXT("A's completion preserves B's one step"), StepsFor(Restarted, TEXT("b")), 1);

        FTapAchievementDesktop OtherGame;
        Configure(OtherGame, Scenario, TEXT("another-game"));
        TestNotEqual(TEXT("Client IDs isolate the same account"), OtherGame.GetQueueFilePath(TEXT("a")), A.GetQueueFilePath(TEXT("a")));
        TestTrue(TEXT("Another client loads its own queue"), OtherGame.LoadQueueFromDisk(TEXT("a")));
        TestEqual(TEXT("Another client cannot replay this client's data"), OtherGame.PendingQueue.Num(), 0);
    }

    // Same-account stale snapshots must neither lose new operations nor resurrect completed ones.
    {
        FTapAchievementDesktop A, B, Restarted;
        Configure(A, TEXT("same-account"));
        Configure(B, TEXT("same-account"));
        Configure(Restarted, TEXT("same-account"));
        A.LoadQueueFromDisk(TEXT("a"));
        B.LoadQueueFromDisk(TEXT("a"));
        A.EnqueuePending(A10);
        B.EnqueuePending(MakeItem(TEXT("a"), TEXT("other-a"), 1));
        TestEqual(TEXT("Stale B merges its delta with A's disk data"), StepsFor(B, TEXT("a")), 11);
        A.FinishPendingReplay(A10.UUID, TEXT("a"), true);
        B.EnqueuePending(MakeItem(TEXT("a"), TEXT("third-a"), 2));
        TestTrue(TEXT("Reload same-account data"), Restarted.LoadQueueFromDisk(TEXT("a")));
        TestEqual(TEXT("B cannot resurrect A's completed ten steps"), StepsFor(Restarted, TEXT("a")), 3);
        TestEqual(TEXT("Both pending increments remain distinct"), Restarted.PendingQueue.Num(), 2);
    }

    // Contended writes retain deltas, and retry merges data published while this instance waited.
    {
        FTapAchievementDesktop A, B, Restarted;
        Configure(A, TEXT("busy"));
        Configure(B, TEXT("busy"));
        Configure(Restarted, TEXT("busy"));
        A.LoadQueueFromDisk(TEXT("a"));
        auto Lease = FTapSharedCacheFile::TryAcquire(A.GetQueueFilePath(TEXT("a")));
        TestTrue(TEXT("Hold account file lock"), !!Lease);
        A.EnqueuePending(A10);
        TestTrue(TEXT("Busy enqueue remains dirty"), A.bQueueNeedsSave);
        Lease.Reset();
        B.EnqueuePending(MakeItem(TEXT("a"), TEXT("other-a"), 1));
        A.SaveQueueToDisk();
        TestFalse(TEXT("Retry commits A's addition"), A.bQueueNeedsSave);
        TestEqual(TEXT("Retry preserves B's concurrent addition"), StepsFor(A, TEXT("a")), 11);

        Lease = FTapSharedCacheFile::TryAcquire(A.GetQueueFilePath(TEXT("a")));
        A.FinishPendingReplay(A10.UUID, TEXT("a"), true);
        TestTrue(TEXT("Busy deletion remains dirty"), A.bQueueNeedsSave);
        TestEqual(TEXT("Successful item is removed from memory immediately"), StepsFor(A, TEXT("a")), 1);
        Lease.Reset();
        B.EnqueuePending(MakeItem(TEXT("a"), TEXT("newer-a"), 2));
        A.SaveQueueToDisk();
        TestTrue(TEXT("Reload after cleanup retry"), Restarted.LoadQueueFromDisk(TEXT("a")));
        TestEqual(TEXT("Cleanup retry deletes only its UUID"), StepsFor(Restarted, TEXT("a")), 3);
    }

    // Import only the matching legacy account. Empty new files prevent reimport after completion.
    {
        FTapAchievementDesktop A, B, Restarted;
        Configure(A, TEXT("migration"));
        Configure(B, TEXT("migration"));
        Configure(Restarted, TEXT("migration"));
        const FString Legacy = TEXT("[{\"achievement_id\":\"steps\",\"steps\":10,\"type\":0,\"uuid\":\"a10\",\"timestamp_ms\":0,\"user_id\":\"a\"},")
            TEXT("{\"achievement_id\":\"steps\",\"steps\":1,\"type\":0,\"uuid\":\"b1\",\"timestamp_ms\":0,\"user_id\":\"b\"}]");
        TestTrue(TEXT("Create legacy shared queue"), FTapSharedCacheFile::Write(A.GetLegacyQueueFilePath(), Legacy));
        auto Lease = FTapSharedCacheFile::TryAcquire(A.GetLegacyQueueFilePath());
        TestFalse(TEXT("A cannot import a locked legacy snapshot"), A.LoadQueueFromDisk(TEXT("a")));
        TestFalse(TEXT("Failed migration does not publish a marker"), IFileManager::Get().FileExists(*A.GetQueueFilePath(TEXT("a"))));
        Lease.Reset();
        TestTrue(TEXT("Import A after releasing the lock"), A.LoadQueueFromDisk(TEXT("a")));
        TestEqual(TEXT("A imports only its ten steps"), StepsFor(A, TEXT("a")), 10);
        A.FinishPendingReplay(A10.UUID, TEXT("a"), true);
        TestTrue(TEXT("Restart reads A's empty marker"), Restarted.LoadQueueFromDisk(TEXT("a")));
        TestEqual(TEXT("Completed legacy data is not resurrected"), Restarted.PendingQueue.Num(), 0);
        TestTrue(TEXT("B can still import after A completed"), B.LoadQueueFromDisk(TEXT("b")));
        TestEqual(TEXT("B retains its legacy step"), StepsFor(B, TEXT("b")), 1);
        FString Preserved;
        FFileHelper::LoadFileToString(Preserved, *A.GetLegacyQueueFilePath());
        TestEqual(TEXT("Legacy backup stays intact"), Preserved, Legacy);
    }

    // Publication failure must retain the uncommitted addition for a later retry.
    {
        FTapAchievementDesktop A, Restarted;
        Configure(A, TEXT("write-failure"));
        Configure(Restarted, TEXT("write-failure"));
        const FString Path = A.GetQueueFilePath(TEXT("a"));
        // A directory at the destination makes atomic file replacement fail on desktop platforms.
        TestTrue(TEXT("Block the destination with a directory"), IFileManager::Get().MakeDirectory(*Path, true));
        A.EnqueuePending(A10);
        TestTrue(TEXT("Publication failure retains the dirty addition"), A.bQueueNeedsSave);
        TestEqual(TEXT("Unsaved operation remains in memory"), StepsFor(A, TEXT("a")), 10);
        TestTrue(TEXT("Remove the empty destination obstruction"), IFileManager::Get().DeleteDirectory(*Path, false, false));
        A.SaveQueueToDisk();
        TestFalse(TEXT("Retry completes publication"), A.bQueueNeedsSave);
        TestTrue(TEXT("Fresh instance can recover after retry"), Restarted.LoadQueueFromDisk(TEXT("a")));
        TestEqual(TEXT("Recovered data preserves the original steps"), StepsFor(Restarted, TEXT("a")), 10);
    }

    // Read/parse failure must never be interpreted as an empty queue and overwritten.
    {
        FTapAchievementDesktop A;
        Configure(A, TEXT("corrupt"));
        const FString Path = A.GetQueueFilePath(TEXT("a"));
        TestTrue(TEXT("Create corrupt queue"), FTapSharedCacheFile::Write(Path, TEXT("{broken")));
        A.EnqueuePending(A10);
        TestTrue(TEXT("Unsaved addition survives a parse failure"), A.bQueueNeedsSave);
        FString Preserved;
        FFileHelper::LoadFileToString(Preserved, *Path);
        TestEqual(TEXT("Corrupt source is not overwritten"), Preserved, FString(TEXT("{broken")));
        TestTrue(TEXT("Repair test fixture"), FTapSharedCacheFile::Write(Path, TEXT("[]")));
        A.SaveQueueToDisk();
        TestFalse(TEXT("Save retries after recovery"), A.bQueueNeedsSave);
        TestEqual(TEXT("The pending addition was retained"), StepsFor(A, TEXT("a")), 10);
    }
    return true;
}
#endif

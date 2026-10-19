// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC)

#include "Desktop/TapAchievementDesktop.h"
#include "TapAchievementCallbackManager.h"
#include "Internal/Http/TapHttp.h"
#include "HAL/FileManager.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementPendingAccountTest,
    "TapSDK.Achievement.PendingQueue.CapturedAccountOwnership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementPendingAccountTest::RunTest(const FString&)
{
    FTapAchievementDesktop Desktop;
    Desktop.QueueFilePathForTest = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"),
        TEXT("achievement-queue-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json"));
    ON_SCOPE_EXIT { IFileManager::Get().Delete(*Desktop.QueueFilePathForTest); };

    Desktop.Initialize(FTapTapSdkOptions());
    TestEqual(TEXT("Achievement tracker uses the packaged module version"),
        Desktop.Tracker.GetProjectVersion(), FString(TapAchievement_VERSION));
    TestEqual(TEXT("Achievement HTTP client uses the packaged module version"),
        Desktop.HttpClient->GetConfig().ModuleVersion, FString(TapAchievement_VERSION));

    // Capture the production HTTP callbacks with A's request-time identity.
    // Mutate the caller's snapshot before completion to verify value ownership.
    FString UserId = TEXT("account_a");
    auto UnlockA = Desktop.CreateUnlockCallback(TEXT("unlock"), TEXT("unlock_a"), UserId, false);
    auto IncrementA = Desktop.CreateIncrementCallback(TEXT("steps"), 7, TEXT("increment_a"), UserId, false);
    auto LogoutUnlockA = Desktop.CreateUnlockCallback(TEXT("logout_unlock"), TEXT("logout_unlock_a"), UserId, false);
    auto LogoutIncrementA = Desktop.CreateIncrementCallback(TEXT("steps"), 3, TEXT("logout_increment_a"), UserId, false);

    UserId = TEXT("account_b");
    UnlockA.Execute(FTapHttpJsonResult::NetworkError());
    IncrementA.Execute(FTapHttpJsonResult::TimeoutError());
    TestEqual(TEXT("B cannot replay A's delayed failures"), Desktop.PendingQueue.BeginReplay(UserId).Num(), 0);

    // B's own operation for the same achievement must remain a separate item.
    auto UnlockB = Desktop.CreateUnlockCallback(TEXT("unlock"), TEXT("unlock_b"), UserId, false);
    UnlockB.Execute(FTapHttpJsonResult::NetworkError());
    const auto ReplayB = Desktop.PendingQueue.BeginReplay(UserId);
    TestEqual(TEXT("B replays only B's unlock"), ReplayB.Num(), 1);
    if (ReplayB.Num() == 1)
    {
        TestEqual(TEXT("B's UUID is preserved"), ReplayB[0].UUID, FString(TEXT("unlock_b")));
    }

    UserId.Empty();
    LogoutUnlockA.Execute(FTapHttpJsonResult::NetworkError());
    LogoutIncrementA.Execute(FTapHttpJsonResult::TimeoutError());
    TestEqual(TEXT("No anonymous items after delayed failure"), Desktop.PendingQueue.BeginReplay(UserId).Num(), 0);
    TestEqual(TEXT("Both accounts' operations remain separate"), Desktop.PendingQueue.Num(), 5);

    // Exercise the real persistence format, using an isolated file.
    FTapAchievementDesktop Reloaded;
    Reloaded.QueueFilePathForTest = Desktop.QueueFilePathForTest;
    Reloaded.LoadQueueFromDisk();
    const auto ReplayA = Reloaded.PendingQueue.BeginReplay(TEXT("account_a"));
    TestEqual(TEXT("A recovers all four operations after reload"), ReplayA.Num(), 4);
    int32 TotalSteps = 0;
    for (const auto& Item : ReplayA)
    {
        TestEqual(TEXT("Persisted owner stays A"), Item.UserId, FString(TEXT("account_a")));
        if (Item.Type == 0) TotalSteps += Item.Steps;
    }
    TestEqual(TEXT("Independent increments preserve their original steps"), TotalSteps, 10);
    TestEqual(TEXT("Replay does not send in-flight items twice"), Reloaded.PendingQueue.BeginReplay(TEXT("account_a")).Num(), 0);
    TestEqual(TEXT("Reload preserves B's separate item"), Reloaded.PendingQueue.BeginReplay(TEXT("account_b")).Num(), 1);

    auto ReplayUnlockA = Reloaded.CreateUnlockCallback(TEXT("unlock"), TEXT("unlock_a"), TEXT("account_a"), true);
    auto ReplayIncrementA = Reloaded.CreateIncrementCallback(TEXT("steps"), 7, TEXT("increment_a"), TEXT("account_a"), true);
    ReplayUnlockA.Execute(FTapHttpJsonResult::NetworkError());
    ReplayIncrementA.Execute(FTapHttpJsonResult::TimeoutError());
    TestEqual(TEXT("Replay failure keeps items without duplicating them"), Reloaded.PendingQueue.Num(), 5);
    const auto RetryA = Reloaded.PendingQueue.BeginReplay(TEXT("account_a"));
    TestEqual(TEXT("Only failed replays become retryable"), RetryA.Num(), 2);
    for (const auto& Item : RetryA)
    {
        TestEqual(TEXT("Replay failure preserves owner"), Item.UserId, FString(TEXT("account_a")));
    }

    // A late success must finish only the matching UUID, leaving B's item intact.
    const auto Success = FTapHttpJsonResult::Success(TSharedPtr<FJsonObject>());
    ReplayUnlockA.Execute(Success);
    ReplayIncrementA.Execute(Success);
    TestEqual(TEXT("Successful replays remove exactly their two items"), Reloaded.PendingQueue.Num(), 3);
    Reloaded.PendingQueue.FinishReplay(TEXT("unlock_b"), false);
    TestEqual(TEXT("A's completion does not remove B's unlock"), Reloaded.PendingQueue.BeginReplay(TEXT("account_b")).Num(), 1);

    return true;
}

#endif

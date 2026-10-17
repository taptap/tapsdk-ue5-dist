#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "TapLeaderboardCallbackManager.h"
#include "Tests/TapLeaderboardCallbackManagerTestTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLeaderboardShareReplacementTest,
    "TapLeaderboard.Callbacks.ShareReplacementAndClear",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapLeaderboardShareReplacementTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> First(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> Second(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());

    Manager.SetShareCallback(First.Get());
    Manager.ShareResult(true, TEXT("first.png"));
    TestEqual(TEXT("Initial receiver gets success"), First->ShareSuccessCount, 1);
    TestEqual(TEXT("Success path preserved"), First->LastShareData, FString(TEXT("first.png")));

    Manager.SetShareCallback(Second.Get());
    Manager.ShareResult(true, TEXT("second.png"));
    Manager.ShareResult(false, TEXT("permission denied"));
    TestEqual(TEXT("Replaced live receiver gets no further success"), First->ShareSuccessCount, 1);
    TestEqual(TEXT("Replaced live receiver gets no failure"), First->ShareFailureCount, 0);
    TestEqual(TEXT("New receiver gets success"), Second->ShareSuccessCount, 1);
    TestEqual(TEXT("New receiver gets failure"), Second->ShareFailureCount, 1);
    TestEqual(TEXT("Failure message preserved"), Second->LastShareData, FString(TEXT("permission denied")));

    Manager.SetShareCallback(Second.Get());
    Manager.ShareResult(true, TEXT("repeat.png"));
    TestEqual(TEXT("Setting the same receiver does not duplicate delivery"), Second->ShareSuccessCount, 2);

    Manager.SetShareCallback(nullptr);
    Manager.ShareResult(true, TEXT("ignored.png"));
    Manager.ShareResult(false, TEXT("ignored error"));
    TestEqual(TEXT("Cleared receiver gets no success"), Second->ShareSuccessCount, 2);
    TestEqual(TEXT("Cleared receiver gets no failure"), Second->ShareFailureCount, 1);

    Manager.SetShareCallback(First.Get());
    Manager.ShareResult(false, TEXT("registered again"));
    TestEqual(TEXT("Can register again after clearing"), First->ShareFailureCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLeaderboardShareQueuedEventsTest,
    "TapLeaderboard.Callbacks.ShareQueuedEvents",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapLeaderboardShareQueuedEventsTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> First(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> Second(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    Manager.SetShareCallback(First.Get());

    // Queue a result from a native worker while the game thread is still in this test.
    Async(EAsyncExecution::ThreadPool, [&Manager]() { Manager.ShareResult(true, TEXT("stale.png")); }).Wait();
    Manager.SetShareCallback(Second.Get());
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestEqual(TEXT("Queued old event does not reach the replaced receiver"), First->ShareSuccessCount, 0);
    TestEqual(TEXT("Queued old event does not reach the new receiver"), Second->ShareSuccessCount, 0);

    Async(EAsyncExecution::ThreadPool, [&Manager]() { Manager.ShareResult(false, TEXT("stale error")); }).Wait();
    Manager.SetShareCallback(nullptr);
    Manager.SetShareCallback(First.Get());
    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestEqual(TEXT("Clear invalidates queued failure even after re-registration"), First->ShareFailureCount, 0);
    TestEqual(TEXT("Cleared receiver gets no queued failure"), Second->ShareFailureCount, 0);

    Manager.ShareResult(true, TEXT("fresh.png"));
    TestEqual(TEXT("New events still arrive after re-registration"), First->ShareSuccessCount, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLeaderboardRequestQueueTest,
    "TapLeaderboard.Callbacks.SerialRequests",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FTapLeaderboardRequestQueueTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> First(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> Second(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    FTapLeaderboardScoresCallback FirstSuccess, SecondSuccess;
    FTapLeaderboardErrorCallback FirstError, SecondError;
    FirstSuccess.BindDynamic(First.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnScores);
    SecondSuccess.BindDynamic(Second.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnScores);
    FirstError.BindDynamic(First.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnError);
    SecondError.BindDynamic(Second.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnError);

    int32 Started = 0, CenteredStarted = 0;
    Manager.EnqueueScores(TEXT("loadScores"), FirstSuccess, FirstError, [&]() { ++Started; });
    Manager.EnqueueScores(TEXT("loadScores"), SecondSuccess, SecondError, [&]() { ++Started; });
    Manager.EnqueueScores(TEXT("loadPlayerCenteredScores"), {}, {}, [&]() { ++CenteredStarted; });
    TestEqual(TEXT("Only first same-operation request starts"), Started, 1);
    TestEqual(TEXT("Different operation starts independently"), CenteredStarted, 1);
    Manager.Error(TEXT("unknown"), -1, TEXT("ignored"));
    TestEqual(TEXT("Unknown operation does not advance queue"), Started, 1);

    FTapLeaderboardScoresResult Result;
    Result.Leaderboard.Id = TEXT("first-board");
    Manager.Scores(TEXT("loadScores"), Result);
    TestEqual(TEXT("Second request starts after first response"), Started, 2);
    TestEqual(TEXT("First receiver gets first payload"), First->LastLeaderboardId, FString(TEXT("first-board")));
    TestEqual(TEXT("Second receiver has not received first payload"), Second->SuccessCount, 0);
    Result.Leaderboard.Id = TEXT("second-board");
    Manager.Scores(TEXT("loadScores"), Result);
    TestEqual(TEXT("Second receiver gets its own payload"), Second->LastLeaderboardId, FString(TEXT("second-board")));
    Manager.Scores(TEXT("loadScores"), Result);
    TestEqual(TEXT("Unsolicited response ignored"), Second->SuccessCount, 1);

    Manager.EnqueueScores(TEXT("loadScores"), FirstSuccess, FirstError, [&]() { ++Started; });
    Manager.EnqueueScores(TEXT("loadScores"), SecondSuccess, SecondError, [&]() { ++Started; });
    Manager.Error(TEXT("loadScores"), 403, TEXT("denied"));
    TestEqual(TEXT("Error advances same-operation queue"), Started, 4);
    TestEqual(TEXT("Error goes to first receiver"), First->ErrorCount, 1);
    TestEqual(TEXT("Error code preserved"), First->LastErrorCode, 403);
    Manager.Scores(TEXT("loadScores"), Result);
    TestEqual(TEXT("Request after failure succeeds"), Second->SuccessCount, 2);

    Manager.Reset();
    int32 PendingStarted = 0;
    Manager.EnqueueScores(TEXT("loadScores"), {}, {}, []() {});
    Manager.EnqueueScores(TEXT("loadScores"), {}, {}, [&]() { ++PendingStarted; });
    Manager.Reset();
    Manager.Scores(TEXT("loadScores"), Result);
    TestEqual(TEXT("Reset drops pending dispatch"), PendingStarted, 0);
    Manager.EnqueueScores(TEXT("loadScores"), {}, {}, [&]() { ++PendingStarted; });
    TestEqual(TEXT("Fresh queue starts after reset"), PendingStarted, 1);
    Manager.Reset();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLeaderboardCurrentAndSubmitQueueTest,
    "TapLeaderboard.Callbacks.CurrentAndSubmitQueues",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FTapLeaderboardCurrentAndSubmitQueueTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    int32 CurrentStarted = 0, SubmitStarted = 0;
    Manager.EnqueueCurrent({}, {}, [&]() { ++CurrentStarted; });
    Manager.EnqueueCurrent({}, {}, [&]() { ++CurrentStarted; });
    Manager.EnqueueCurrent({}, {}, [&]() { ++CurrentStarted; });
    Manager.EnqueueSubmit({}, {}, [&]() { ++SubmitStarted; });
    Manager.EnqueueSubmit({}, {}, [&]() { ++SubmitStarted; });
    Manager.EnqueueSubmit({}, {}, [&]() { ++SubmitStarted; });
    TestEqual(TEXT("Current request is serialized"), CurrentStarted, 1);
    TestEqual(TEXT("Submit runs independently and is serialized"), SubmitStarted, 1);
    Manager.Current({});
    TestEqual(TEXT("Current success advances"), CurrentStarted, 2);
    Manager.Error(TEXT("loadCurrentPlayerScore"), -1, TEXT("failure"));
    TestEqual(TEXT("Current error advances"), CurrentStarted, 3);
    Manager.Submitted({});
    TestEqual(TEXT("Submit success advances"), SubmitStarted, 2);
    Manager.Error(TEXT("submitScores"), -1, TEXT("failure"));
    TestEqual(TEXT("Submit error advances"), SubmitStarted, 3);
    Manager.Reset();
    Manager.Current({});
    Manager.Submitted({});
    TestEqual(TEXT("Current reset keeps queue empty"), CurrentStarted, 3);
    TestEqual(TEXT("Submit reset keeps queue empty"), SubmitStarted, 3);
    return true;
}
#endif

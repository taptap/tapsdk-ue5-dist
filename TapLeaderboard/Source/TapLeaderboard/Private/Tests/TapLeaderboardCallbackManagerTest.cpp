#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "TapLeaderboardCallbackManager.h"
#include "Tests/TapLeaderboardCallbackManagerTestTypes.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"
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
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapLeaderboardRequestQueueTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> First(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    TStrongObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> Second(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    FTapLeaderboardScoresCallback FirstSuccess, SecondSuccess;
    FTapLeaderboardErrorCallback FirstError, SecondError;
    FirstSuccess.BindUObject(First.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnScores);
    SecondSuccess.BindUObject(Second.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnScores);
    FirstError.BindUObject(First.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnError);
    SecondError.BindUObject(Second.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnError);

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
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapLeaderboardCurrentAndSubmitQueueTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    int32 CurrentSuccesses = 0, SubmitSuccesses = 0, Failures = 0;
    const auto CurrentSuccess = FTapLeaderboardCurrentScoreCallback::CreateLambda(
        [&](const FTapUserScoreResult&) { ++CurrentSuccesses; });
    const auto SubmitSuccess = FTapLeaderboardSubmitCallback::CreateLambda(
        [&](const FTapSubmitScoresResult&) { ++SubmitSuccesses; });
    const auto Error = FTapLeaderboardErrorCallback::CreateLambda(
        [&](const FString&, int32, const FString&) { ++Failures; });
    int32 CurrentStarted = 0, SubmitStarted = 0;
    Manager.EnqueueCurrent(CurrentSuccess, Error, [&]() { ++CurrentStarted; });
    Manager.EnqueueCurrent(CurrentSuccess, Error, [&]() { ++CurrentStarted; });
    Manager.EnqueueCurrent(CurrentSuccess, Error, [&]() { ++CurrentStarted; });
    Manager.EnqueueSubmit(SubmitSuccess, Error, [&]() { ++SubmitStarted; });
    Manager.EnqueueSubmit(SubmitSuccess, Error, [&]() { ++SubmitStarted; });
    Manager.EnqueueSubmit(SubmitSuccess, Error, [&]() { ++SubmitStarted; });
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
    TestEqual(TEXT("Current success delivered once"), CurrentSuccesses, 1);
    TestEqual(TEXT("Submit success delivered once"), SubmitSuccesses, 1);
    TestEqual(TEXT("Each failed request delivered once"), Failures, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLeaderboardRequestLifetimeTest,
    "TapLeaderboard.Callbacks.RequestLifetime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapLeaderboardRequestLifetimeTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    TWeakPtr<int32> Captured;
    int32 Successes = 0, Failures = 0;
    auto Enqueue = [&]()
    {
        const auto State = MakeShared<int32>(42);
        Captured = State;
        Manager.EnqueueScores(TEXT("loadScores"),
            FTapLeaderboardScoresCallback::CreateLambda([&, State](const FTapLeaderboardScoresResult&)
            {
                TestEqual(TEXT("Temporary delegate capture survives until delivery"), *State, 42);
                ++Successes;
            }),
            FTapLeaderboardErrorCallback::CreateLambda([&, State](const FString&, int32 Code, const FString&)
            {
                TestEqual(TEXT("Error capture survives until delivery"), *State, 42);
                TestEqual(TEXT("Error code preserved"), Code, 403);
                ++Failures;
            }), []() {});
    };
    Enqueue();
    TestTrue(TEXT("Manager owns a copy after caller delegate expires"), Captured.IsValid());
    Manager.Scores(TEXT("loadScores"), {});
    TestFalse(TEXT("Success releases both delegate captures"), Captured.IsValid());
    Enqueue();
    Manager.Error(TEXT("loadScores"), 403, TEXT("denied"));
    TestFalse(TEXT("Failure releases both delegate captures"), Captured.IsValid());
    Enqueue();
    Manager.Reset();
    TestFalse(TEXT("Reset releases pending captures"), Captured.IsValid());
    Manager.Scores(TEXT("loadScores"), {});
    Manager.Error(TEXT("loadScores"), 403, TEXT("late"));
    TestEqual(TEXT("Only completed success delivered"), Successes, 1);
    TestEqual(TEXT("Only completed failure delivered"), Failures, 1);

    // Weak bindings must not retain destroyed UI receivers or block later requests.
    TWeakObjectPtr<UTapLeaderboardCallbackManagerTestReceiver> Receiver(NewObject<UTapLeaderboardCallbackManagerTestReceiver>());
    int32 WeakCalls = 0, NextStarted = 0;
    Manager.EnqueueScores(TEXT("loadScores"),
        FTapLeaderboardScoresCallback::CreateUObject(Receiver.Get(), &UTapLeaderboardCallbackManagerTestReceiver::OnScores),
        {}, []() {});
    Manager.EnqueueScores(TEXT("loadScores"), {},
        FTapLeaderboardErrorCallback::CreateWeakLambda(Receiver.Get(),
            [&](const FString&, int32, const FString&) { ++WeakCalls; }), []() {});
    Manager.EnqueueScores(TEXT("loadScores"),
        FTapLeaderboardScoresCallback::CreateLambda([&](const FTapLeaderboardScoresResult&) { ++Successes; }),
        {}, [&]() { ++NextStarted; });
    CollectGarbage(RF_NoFlags);
    TestFalse(TEXT("Native UObject delegates do not keep receivers alive"), Receiver.IsValid());
    Manager.Scores(TEXT("loadScores"), {});
    Manager.Error(TEXT("loadScores"), 403, TEXT("receiver gone"));
    TestEqual(TEXT("Expired weak lambda is skipped"), WeakCalls, 0);
    TestEqual(TEXT("Expired receivers do not block the queue"), NextStarted, 1);
    Manager.Scores(TEXT("loadScores"), {});
    TestEqual(TEXT("Next live request still receives its result"), Successes, 2);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLeaderboardRequestReentrantTest,
    "TapLeaderboard.Callbacks.RequestReentrancy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapLeaderboardRequestReentrantTest::RunTest(const FString&)
{
    FTapLeaderboardCallbackManager Manager;
    int32 Started = 0, NextCompleted = 0, ReentrantCompleted = 0;
    Manager.EnqueueScores(TEXT("loadScores"),
        FTapLeaderboardScoresCallback::CreateLambda([&](const FTapLeaderboardScoresResult&)
        {
            Manager.EnqueueScores(TEXT("loadScores"),
                FTapLeaderboardScoresCallback::CreateLambda([&](const FTapLeaderboardScoresResult&) { ++ReentrantCompleted; }),
                {}, [&]() { ++Started; });
        }), {}, [&]() { ++Started; });
    Manager.EnqueueScores(TEXT("loadScores"),
        FTapLeaderboardScoresCallback::CreateLambda([&](const FTapLeaderboardScoresResult&) { ++NextCompleted; }),
        {}, [&]() { ++Started; });
    Manager.Scores(TEXT("loadScores"), {});
    TestEqual(TEXT("Reentrant request waits behind reserved next request"), Started, 2);
    Manager.Scores(TEXT("loadScores"), {});
    TestEqual(TEXT("Reentrant request starts after next completion"), Started, 3);
    TestEqual(TEXT("Next request receives its own result"), NextCompleted, 1);
    TestEqual(TEXT("Reentrant request has not received another result"), ReentrantCompleted, 0);
    Manager.Scores(TEXT("loadScores"), {});
    TestEqual(TEXT("Reentrant request completes once"), ReentrantCompleted, 1);
    return true;
}
#endif

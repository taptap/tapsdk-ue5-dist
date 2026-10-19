// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "TapRelationLiteCallbackUtils.h"
#include "TapRelationLiteListResult.h"
#include "Async/Async.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapRelationLiteResultCodeTest,
    "TapRelationLite.Parser.ResultCode", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapRelationLiteResultCodeTest::RunTest(const FString&)
{
    int32 Code = 0;
    TestTrue(TEXT("Swift numeric code parses"), TapRelationLiteCallbackUtils::ParseResultCode(TEXT("{\"relation_lite_result_code\":700001}"), Code));
    TestEqual(TEXT("Numeric code preserved"), Code, 700001);
    TestTrue(TEXT("Legacy string code parses"), TapRelationLiteCallbackUtils::ParseResultCode(TEXT("{\"relation_lite_result_code\":\"700002\"}"), Code));
    TestEqual(TEXT("String code preserved"), Code, 700002);
    for (const FString Json : {TEXT("{}"), TEXT("invalid"), TEXT("{\"relation_lite_result_code\":null}"), TEXT("{\"relation_lite_result_code\":\"invalid\"}")})
        TestFalse(TEXT("Invalid event rejected"), TapRelationLiteCallbackUtils::ParseResultCode(Json, Code));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapRelationLiteSyncCallbackTest,
    "TapRelationLite.Callbacks.SyncGameThread", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapRelationLiteSyncCallbackTest::RunTest(const FString&)
{
    struct FState
    {
        std::atomic<int32> Successes{0}, Failures{0};
        std::atomic<bool> bCorrect{true};
    };
    const auto State = MakeShared<FState, ESPMode::ThreadSafe>();
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [State]
    {
        const TArray<FString> Responses = {TEXT("{\"success\":0}"), TEXT("{\"success\":-1,\"error_message\":\"not login\"}"),
            TEXT("invalid"), TEXT("{}"), TEXT("{\"success\":0.5}"), TEXT("{\"success\":0,\"error_message\":\"native error\"}")};
        for (int32 Index = 0; Index < Responses.Num(); ++Index)
        {
            TapRelationLiteCallbackUtils::CompleteSync(Responses[Index], [State, Index]
            {
                if (!IsInGameThread() || Index != 0) State->bCorrect = false;
                ++State->Successes;
            }, [State, Index](const FString& Error)
            {
                if (!IsInGameThread() || Index == 0 || Error.IsEmpty() ||
                    (Index == 1 && Error != TEXT("not login"))) State->bCorrect = false;
                ++State->Failures;
            });
        }
    });
    const double Deadline = FPlatformTime::Seconds() + 10;
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Deadline]
    {
        if (State->Successes + State->Failures < 6 && FPlatformTime::Seconds() < Deadline) return false;
        TestEqual(TEXT("Only success=0 succeeds"), State->Successes.load(), 1);
        TestEqual(TEXT("Errors and malformed responses fail"), State->Failures.load(), 5);
        TestTrue(TEXT("Correct result and GameThread delivery"), State->bCorrect.load());
        return true;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapRelationLiteCallbackLifetimeTest,
    "TapRelationLite.Callbacks.RegistrationAndShutdown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapRelationLiteCallbackLifetimeTest::RunTest(const FString&)
{
    struct FState
    {
        FTapRelationLiteCallbackGeneration Relation, Invite, Lifetime;
        int32 OldCalls = 0, NewCalls = 0;
        bool bBarrier = false;
    };
    const auto State = MakeShared<FState>();
    const auto OldRelation = State->Relation.Capture();
    const auto OldInvite = State->Invite.Capture();
    const auto OldLifetime = State->Lifetime.Capture();

    // Invalidate from inside a GameThread task, before that queue can run the callbacks.
    TapRelationLiteCallbackUtils::Dispatch([State, OldRelation, OldInvite, OldLifetime]
    {
        const auto OldCall = [State] { ++State->OldCalls; };
        TapRelationLiteCallbackUtils::Dispatch(OldCall, OldRelation);
        TapRelationLiteCallbackUtils::Dispatch(OldCall, OldInvite);
        TapRelationLiteCallbackUtils::CompleteSync(TEXT("{\"success\":0}"), OldCall,
            [State](const FString&) { ++State->OldCalls; }, OldLifetime);
        TapRelationLiteListResult::Complete(TEXT("{\"next_page_token\":\"\",\"friends_list\":\"[]\"}"), TEXT("friends_list"),
            [State](const FTapRelationLiteUserResult&) { ++State->OldCalls; },
            [State](int32, const FString&) { ++State->OldCalls; }, OldLifetime);
        State->Relation.Invalidate();
        State->Invite.Invalidate();
        State->Lifetime.Invalidate();
        // A response that arrives only after shutdown must also be dropped.
        TapRelationLiteListResult::Complete(TEXT("{\"error_message\":\"late failure\"}"), TEXT("friends_list"),
            [State](const FTapRelationLiteUserResult&) { ++State->OldCalls; },
            [State](int32, const FString&) { ++State->OldCalls; }, OldLifetime);
        const auto SupersededRegistration = State->Relation.Capture();
        State->Relation.Invalidate();
        TapRelationLiteCallbackUtils::Dispatch(OldCall, SupersededRegistration);
        TapRelationLiteCallbackUtils::Dispatch([State] { ++State->NewCalls; }, State->Relation.Capture());
        TapRelationLiteCallbackUtils::Dispatch([State] { ++State->NewCalls; }, State->Invite.Capture());
        TapRelationLiteCallbackUtils::Dispatch([State] { State->bBarrier = true; });
    });
    const double Deadline = FPlatformTime::Seconds() + 10;
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, OldRelation, OldInvite, OldLifetime, Deadline]
    {
        if (!State->bBarrier && FPlatformTime::Seconds() < Deadline) return false;
        TestTrue(TEXT("GameThread queue drained"), State->bBarrier);
        TestFalse(TEXT("Old relation registration stays invalid"), OldRelation());
        TestFalse(TEXT("Old invite registration stays invalid"), OldInvite());
        TestFalse(TEXT("Old request lifetime stays invalid"), OldLifetime());
        TestEqual(TEXT("Old queued events and late requests are suppressed"), State->OldCalls, 0);
        TestEqual(TEXT("New registrations still receive events"), State->NewCalls, 2);
        return true;
    }));
    return true;
}
#endif

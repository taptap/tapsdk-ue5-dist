// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "TapRelationLiteListResult.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include <atomic>

namespace
{
    FString ListResponse(const FString& Key, const FString& List, const FString& Token = TEXT(""))
    {
        const auto Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("next_page_token"), Token);
        Root->SetStringField(Key, List);
        FString Json;
        const auto Writer = TJsonWriterFactory<>::Create(&Json);
        FJsonSerializer::Serialize(Root, Writer);
        return Json;
    }
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FTapRelationLiteListResultTest,
    "TapRelationLite.Parser.IOSListResult",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FTapRelationLiteListResultTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
    for (const FString Key : {TEXT("friends_list"), TEXT("following_list"), TEXT("fans_list")})
    {
        Names.Add(Key);
        Commands.Add(Key);
    }
}

bool FTapRelationLiteListResultTest::RunTest(const FString& Key)
{
    using TapRelationLiteListResult::Parse;
    const auto Empty = Parse(ListResponse(Key, TEXT("[]")), Key);
    TestTrue(TEXT("A valid empty list succeeds"), Empty.bSuccess);
    TestEqual(TEXT("Empty list has no users"), Empty.Result.Users.Num(), 0);
    TestTrue(TEXT("Empty next token is valid"), Empty.Result.NextPageToken.IsEmpty());

    const FString UserJson = TEXT(R"({"created_at":9007199254740993,"user":{"open_id":"user-a","name":"Alice","alias":"A","avatar":"avatar-url"},"follow_status":{"blocked_by":false,"blocking":true,"followed_by":true,"following":false}})");
    const auto Success = Parse(ListResponse(Key, TEXT("[") + UserJson + TEXT("]"), TEXT("next-page")), Key);
    TestTrue(TEXT("Native list item succeeds"), Success.bSuccess);
    TestEqual(TEXT("Response token is preserved"), Success.Result.NextPageToken, FString(TEXT("next-page")));
    TestEqual(TEXT("One user returned"), Success.Result.Users.Num(), 1);
    if (Success.Result.Users.Num() == 1)
    {
        const auto& Item = Success.Result.Users[0];
        TestEqual(TEXT("CreatedAt retains int64 precision"), Item.CreatedAt, int64(9007199254740993LL));
        TestEqual(TEXT("Open ID preserved"), Item.User.OpenId, FString(TEXT("user-a")));
        TestEqual(TEXT("Name preserved"), Item.User.Name, FString(TEXT("Alice")));
        TestEqual(TEXT("Alias preserved"), Item.User.Alias, FString(TEXT("A")));
        TestEqual(TEXT("Avatar preserved"), Item.User.Avatar, FString(TEXT("avatar-url")));
        TestTrue(TEXT("Follow status preserved"), !Item.FollowStatus.bBlockedBy && Item.FollowStatus.bBlocking &&
            Item.FollowStatus.bFollowedBy && !Item.FollowStatus.bFollowing);
    }

    for (const FString Message : {TEXT("network unavailable"), TEXT("not login"), TEXT("no user_friends scope")})
    {
        const auto Error = Parse(TEXT("{\"error_message\":\"") + Message + TEXT("\"}"), Key);
        TestFalse(TEXT("Native failure must not succeed"), Error.bSuccess);
        TestEqual(TEXT("Native error message preserved"), Error.ErrorMessage, Message);
    }
    const auto Mixed = Parse(TEXT("{\"error_message\":\"native error\",\"next_page_token\":\"\",\"") + Key + TEXT("\":\"[]\"}"), Key);
    TestFalse(TEXT("Error takes precedence over success fields"), Mixed.bSuccess);

    const TArray<FString> InvalidResponses = {
        TEXT(""), TEXT("invalid"), TEXT("null"), TEXT("[]"), TEXT("{}"),
        TEXT("{\"error_message\":\"\"}"), TEXT("{\"error_message\":null}"),
        TEXT("{\"next_page_token\":\"\"}"),
        TEXT("{\"") + Key + TEXT("\":\"[]\"}"),
        TEXT("{\"next_page_token\":null,\"") + Key + TEXT("\":\"[]\"}"),
        TEXT("{\"next_page_token\":\"\",\"") + Key + TEXT("\":[]}"),
        ListResponse(TEXT("wrong_list"), TEXT("[]")),
        ListResponse(Key, TEXT("")), ListResponse(Key, TEXT("invalid")),
        ListResponse(Key, TEXT("{}")), ListResponse(Key, TEXT("null")),
        ListResponse(Key, TEXT("[null]")), ListResponse(Key, TEXT("[1]")),
        ListResponse(Key, TEXT("[{}]")),
        ListResponse(Key, TEXT("[{\"created_at\":0,\"user\":null,\"follow_status\":{}}]")),
        ListResponse(Key, TEXT("[{\"created_at\":0,\"user\":{},\"follow_status\":false}]")),
        ListResponse(Key, TEXT("[") + UserJson + TEXT(",null]"))
    };
    for (const auto& Json : InvalidResponses)
    {
        const auto Invalid = Parse(Json, Key);
        TestFalse(TEXT("Malformed/missing data must fail: ") + Json, Invalid.bSuccess);
        TestFalse(TEXT("Failure has an error description"), Invalid.ErrorMessage.IsEmpty());
        TestEqual(TEXT("Failure does not leak a partially parsed list"), Invalid.Result.Users.Num(), 0);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapRelationLiteListDispatchTest,
    "TapRelationLite.IOSListResult.GameThreadCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapRelationLiteListDispatchTest::RunTest(const FString&)
{
    struct FState
    {
        std::atomic<int32> Successes{0};
        std::atomic<int32> Failures{0};
        std::atomic<bool> bCorrect{true};
    };
    const auto State = MakeShared<FState, ESPMode::ThreadSafe>();
    AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [State]
    {
        for (const FString Key : {TEXT("friends_list"), TEXT("following_list"), TEXT("fans_list")})
        {
            const TArray<FString> Responses = {ListResponse(Key, TEXT("[]")), TEXT("{\"error_message\":\"network down\"}"), TEXT("invalid")};
            for (int32 Index = 0; Index < Responses.Num(); ++Index)
            {
                TapRelationLiteListResult::Complete(Responses[Index], Key,
                    [State, Index](const FTapRelationLiteUserResult& Result)
                    {
                        if (!IsInGameThread() || Index != 0 || Result.Users.Num() != 0 || !Result.NextPageToken.IsEmpty())
                            State->bCorrect = false;
                        ++State->Successes;
                    },
                    [State, Index](int32 Code, const FString& Message)
                    {
                        if (!IsInGameThread() || Index == 0 || Code != -1 || Message.IsEmpty() ||
                            (Index == 1 && Message != TEXT("network down"))) State->bCorrect = false;
                        ++State->Failures;
                    });
            }
        }
    });
    const double Deadline = FPlatformTime::Seconds() + 10.0;
    ADD_LATENT_AUTOMATION_COMMAND(FFunctionLatentCommand([this, State, Deadline]
    {
        if (State->Successes + State->Failures < 9 && FPlatformTime::Seconds() < Deadline) return false;
        TestEqual(TEXT("Exactly one success per valid empty list"), State->Successes.load(), 3);
        TestEqual(TEXT("Native and parse errors invoke only failure"), State->Failures.load(), 6);
        TestTrue(TEXT("Callbacks preserve results and run on GameThread"), State->bCorrect.load());
        return true;
    }));
    return true;
}
#endif

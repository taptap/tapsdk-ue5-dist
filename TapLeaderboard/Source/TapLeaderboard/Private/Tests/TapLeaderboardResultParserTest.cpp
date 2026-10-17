// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapLeaderboardResultParser.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapLeaderboardScoresResultParserTest,
    "TapLeaderboard.Parser.ScoresResult",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FTapLeaderboardScoresResultParserTest::RunTest(const FString&)
{
    const FString Json = TEXT(R"JSON(
        {
            "leaderboard": {
                "available_periods": [
                    {"display":"Season 1","period_token":"season-1"},
                    null,
                    "invalid",
                    {"display":"Season 2","period_token":"season-2"}
                ],
                "id":"leaderboard-1",
                "name":"Global Scores",
                "period":{"display":"Season 2","period_token":"season-2"},
                "score":{
                    "rank":7,
                    "rank_display":"#7",
                    "score":9007199254740991,
                    "score_display":"9,007,199,254,740,991",
                    "score_submitted_time":1750000000000,
                    "user":{
                        "openid":"open-1",
                        "unionid":"union-1",
                        "name":"Player One",
                        "avatar":"https://example.com/avatar.png"
                    }
                }
            },
            "scores": [
                {
                    "rank":1,
                    "rank_display":"#1",
                    "score":1000,
                    "score_display":"1,000",
                    "score_submitted_time":1750000000001,
                    "user":{
                        "openid":"open-2",
                        "unionid":"union-2",
                        "name":"Player Two",
                        "avatar":"https://example.com/player-two.png"
                    }
                }
            ],
            "next_page":"next-token"
        }
    )JSON");

    const FTapLeaderboardScoresResult Result = TapLeaderboardResultParser::ParseScoresResult(Json);

    TestEqual(TEXT("Leaderboard ID"), Result.Leaderboard.Id, FString(TEXT("leaderboard-1")));
    TestEqual(TEXT("Leaderboard name"), Result.Leaderboard.Name, FString(TEXT("Global Scores")));
    TestEqual(TEXT("Invalid period entries are skipped"), Result.Leaderboard.AvailablePeriods.Num(), 2);
    if (Result.Leaderboard.AvailablePeriods.Num() == 2)
    {
        TestEqual(TEXT("First period display"), Result.Leaderboard.AvailablePeriods[0].Display, FString(TEXT("Season 1")));
        TestEqual(TEXT("Second period token"), Result.Leaderboard.AvailablePeriods[1].PeriodToken, FString(TEXT("season-2")));
    }
    TestEqual(TEXT("Current period display"), Result.Leaderboard.Period.Display, FString(TEXT("Season 2")));
    TestEqual(TEXT("Current period token"), Result.Leaderboard.Period.PeriodToken, FString(TEXT("season-2")));
    TestEqual(TEXT("Leaderboard score rank"), Result.Leaderboard.Score.Rank, static_cast<int64>(7));
    TestEqual(TEXT("Leaderboard score"), Result.Leaderboard.Score.Score, static_cast<int64>(9007199254740991LL));
    TestEqual(TEXT("Leaderboard score avatar"), Result.Leaderboard.Score.User.AvatarUrl,
        FString(TEXT("https://example.com/avatar.png")));
    TestEqual(TEXT("Scores count"), Result.Scores.Num(), 1);
    if (Result.Scores.Num() == 1)
    {
        TestEqual(TEXT("List score avatar"), Result.Scores[0].User.AvatarUrl,
            FString(TEXT("https://example.com/player-two.png")));
    }
    TestEqual(TEXT("Next page"), Result.NextPage, FString(TEXT("next-token")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapLeaderboardUserScoreResultParserTest,
    "TapLeaderboard.Parser.UserScoreResult",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FTapLeaderboardUserScoreResultParserTest::RunTest(const FString&)
{
    const FString Json = TEXT(R"JSON(
        {
            "leaderboard": {
                "available_periods": [
                    {"display":"Daily","period_token":"daily-token"}
                ],
                "id":"leaderboard-2",
                "name":"Daily Scores",
                "period":{"display":"Daily","period_token":"daily-token"},
                "score":{
                    "rank":4,
                    "score":400,
                    "user":{"avatar":{"url":"https://example.com/legacy.png"}}
                }
            },
            "current_user_score":{
                "rank":5,
                "rank_display":"#5",
                "score":350,
                "score_display":"350",
                "score_submitted_time":1750000000002,
                "user":{
                    "openid":"open-current",
                    "unionid":"union-current",
                    "name":"Current Player",
                    "avatar":"https://example.com/current.png"
                }
            }
        }
    )JSON");

    const FTapUserScoreResult Result = TapLeaderboardResultParser::ParseUserScoreResult(Json);

    TestEqual(TEXT("Available periods"), Result.Leaderboard.AvailablePeriods.Num(), 1);
    TestEqual(TEXT("Current period token"), Result.Leaderboard.Period.PeriodToken, FString(TEXT("daily-token")));
    TestEqual(TEXT("Legacy object avatar remains supported"), Result.Leaderboard.Score.User.AvatarUrl,
        FString(TEXT("https://example.com/legacy.png")));
    TestEqual(TEXT("Current user score"), Result.CurrentUserScore.Score, static_cast<int64>(350));
    TestEqual(TEXT("Native string avatar"), Result.CurrentUserScore.User.AvatarUrl,
        FString(TEXT("https://example.com/current.png")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapLeaderboardOptionalFieldsParserTest,
    "TapLeaderboard.Parser.OptionalFields",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FTapLeaderboardOptionalFieldsParserTest::RunTest(const FString&)
{
    const FString NullJson = TEXT(R"JSON(
        {
            "leaderboard": {
                "id":"leaderboard-null",
                "available_periods": null,
                "period": null,
                "score": null
            },
            "current_user_score": {
                "user": {"avatar": null}
            }
        }
    )JSON");

    const FTapUserScoreResult NullResult = TapLeaderboardResultParser::ParseUserScoreResult(NullJson);
    TestEqual(TEXT("Null periods become empty array"), NullResult.Leaderboard.AvailablePeriods.Num(), 0);
    TestTrue(TEXT("Null period becomes default value"), NullResult.Leaderboard.Period.Display.IsEmpty());
    TestEqual(TEXT("Null score becomes default value"), NullResult.Leaderboard.Score.Score, static_cast<int64>(0));
    TestTrue(TEXT("Null avatar becomes empty string"), NullResult.CurrentUserScore.User.AvatarUrl.IsEmpty());

    const FString MissingJson = TEXT(R"JSON({"leaderboard":{"id":"leaderboard-missing"}})JSON");
    const FTapLeaderboardScoresResult MissingResult = TapLeaderboardResultParser::ParseScoresResult(MissingJson);
    TestEqual(TEXT("Missing periods become empty array"), MissingResult.Leaderboard.AvailablePeriods.Num(), 0);
    TestTrue(TEXT("Missing period token is empty"), MissingResult.Leaderboard.Period.PeriodToken.IsEmpty());
    TestEqual(TEXT("Missing score is default"), MissingResult.Leaderboard.Score.Rank, static_cast<int64>(0));
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

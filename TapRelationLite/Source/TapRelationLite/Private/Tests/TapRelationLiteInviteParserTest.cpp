#include "Misc/AutomationTest.h"
#include "TapRelationLiteInviteParser.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapRelationLiteInviteParserTest,
    "TapRelationLite.Parser.NativeInvite",
    EAutomationTestFlags::ClientContext | EAutomationTestFlags::SmokeFilter)

bool FTapRelationLiteInviteParserTest::RunTest(const FString&)
{
    FString OpenId, UnionId, TeamId;
    TestTrue(TEXT("Team invite parses"), TapRelationLiteInviteParser::Parse(
        TEXT(R"({"invite_type":"invite_team","open_id":"open-1","union_id":"union-1","team_id":"team-1"})"),
        OpenId, UnionId, TeamId));
    TestEqual(TEXT("Sender open ID"), OpenId, FString(TEXT("open-1")));
    TestEqual(TEXT("Sender union ID"), UnionId, FString(TEXT("union-1")));
    TestEqual(TEXT("Team ID"), TeamId, FString(TEXT("team-1")));
    TestTrue(TEXT("Game invite parses"), TapRelationLiteInviteParser::Parse(
        TEXT(R"({"invite_type":"invite_game","open_id":"open-2","union_id":"union-2"})"),
        OpenId, UnionId, TeamId));
    TestEqual(TEXT("Game invite sender"), OpenId, FString(TEXT("open-2")));
    TestTrue(TEXT("Game invite clears previous team"), TeamId.IsEmpty());
    TestFalse(TEXT("Malformed JSON rejected"), TapRelationLiteInviteParser::Parse(TEXT("invalid"), OpenId, UnionId, TeamId));
    TestTrue(TEXT("Malformed input clears IDs"), OpenId.IsEmpty() && UnionId.IsEmpty() && TeamId.IsEmpty());
    return true;
}
#endif

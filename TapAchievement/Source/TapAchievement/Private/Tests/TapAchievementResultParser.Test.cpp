// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapAchievement.h"
#include "TapAchievementResultParser.h"
#include "TapAchievementToastWidget.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Internal/Http/TapHttpParser.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"

namespace
{
    const int64 StepValues[] = {
        0, MAX_int32, int64(MAX_int32) + 1, 9007199254740993LL, MAX_int64, MIN_int64
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementMobileStepsTest,
    "TapSDK.Achievement.Result.MobileInt64",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementMobileStepsTest::RunTest(const FString&)
{
    for (int64 Steps : StepValues)
    {
        for (bool bQuotedSteps : {false, true})
        {
            const FString Value = LexToString(Steps);
            const FString Result = FString::Printf(
                TEXT("{\"achievementId\":\"steps\",\"achievementName\":\"Progress\",\"achievementType\":\"PLATINUM\",\"currentSteps\":%s}"),
                *(bQuotedSteps ? TEXT("\"") + Value + TEXT("\"") : Value));
            for (bool bNestedString : {false, true})
            {
                FString Payload;
                if (bNestedString)
                {
                    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
                    Root->SetStringField(TEXT("state"), TEXT("success"));
                    Root->SetNumberField(TEXT("code"), ETapAchievementError::IncrementSuccess);
                    Root->SetStringField(TEXT("result"), Result);
                    auto Writer = TJsonWriterFactory<>::Create(&Payload);
                    FJsonSerializer::Serialize(Root, Writer);
                }
                else
                {
                    Payload = TEXT("{\"state\":\"success\",\"code\":70002,\"result\":") + Result + TEXT("}");
                }

                const auto Parsed = TapAchievementResultParser::Parse(Payload);
                TestTrue(TEXT("Success callback parses"), Parsed.bParsed && Parsed.bIsSuccess);
                TestEqual(TEXT("Code"), Parsed.Code, ETapAchievementError::IncrementSuccess);
                TestEqual(TEXT("Id"), Parsed.Result.AchievementId, FString(TEXT("steps")));
                TestEqual(TEXT("Type"), Parsed.Result.AchievementType, ETapAchievementType::Platinum);
                TestEqual(*Payload, Parsed.Result.CurrentSteps, Steps);
            }
        }
    }
    TestEqual(TEXT("Missing steps defaults to zero"),
        TapAchievementResultParser::Parse(TEXT("{\"state\":\"success\",\"result\":{}}" )).Result.CurrentSteps, int64(0));
    TestFalse(TEXT("Invalid JSON is rejected"), TapAchievementResultParser::Parse(TEXT("invalid")).bParsed);
    const auto Failure = TapAchievementResultParser::Parse(
        TEXT("{\"state\":\"failure\",\"achievementId\":\"steps\",\"errorCode\":80020,\"errorMsg\":\"invalid\"}"));
    TestTrue(TEXT("Failure callback parses"), Failure.bParsed && !Failure.bIsSuccess);
    TestEqual(TEXT("Failure code"), Failure.ErrorCode, ETapAchievementError::InvalidRequest);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementDesktopStepsTest,
    "TapSDK.Achievement.Result.DesktopInt64",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementDesktopStepsTest::RunTest(const FString&)
{
    auto Parser = FTapHttpParser::CreateEnvelopeParser(true);
    for (int64 Steps : StepValues)
    {
        for (ETapAchievementType Type : {ETapAchievementType::Normal, ETapAchievementType::Platinum})
        {
            const FString Field = Type == ETapAchievementType::Normal ? TEXT("achievement") : TEXT("platinum_achievement");
            const FString Payload = FString::Printf(
                TEXT("{\"success\":true,\"data\":{\"%s\":{\"id\":\"steps\",\"name\":\"Progress\",\"current_steps\":%lld}}}"),
                *Field, Steps);
            const auto HttpResult = Parser->Parse(200, Payload, true, FString());
            if (!TestTrue(TEXT("HTTP result parses"), HttpResult.IsSuccess() && HttpResult.GetData().IsValid()))
            {
                return false;
            }
            FTapAchievementResult Result;
            TestTrue(TEXT("Achievement bean parses"), TapAchievementResultParser::ParseAchievementBean(
                HttpResult.GetData()->GetObjectField(Field), Type, Result));
            TestEqual(TEXT("Id"), Result.AchievementId, FString(TEXT("steps")));
            TestEqual(TEXT("Type"), Result.AchievementType, Type);
            TestEqual(*Payload, Result.CurrentSteps, Steps);
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementApiBoundaryTest,
    "TapSDK.Achievement.PublicApi.BusinessFacade",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementApiBoundaryTest::RunTest(const FString&)
{
    const UClass* Facade = UTapAchievement::StaticClass();
    TestNull(TEXT("Toast completion is absent from facade"), Facade->FindFunctionByName(TEXT("NotifyToastFinished")));
    TestNull(TEXT("Debug preview is absent from facade"), Facade->FindFunctionByName(TEXT("DebugShowToast")));
    for (const TCHAR* Method : {TEXT("RegisterCallback"), TEXT("UnregisterCallback"), TEXT("Unlock"),
                               TEXT("Increment"), TEXT("ShowAchievements"), TEXT("SetToastEnable")})
    {
        TestNotNull(Method, Facade->FindFunctionByName(Method));
    }
    TestNotNull(TEXT("Widget retains toast completion"),
        UTapAchievementToastWidget::StaticClass()->FindFunctionByName(TEXT("NotifyAnimationFinished")));
    const FInt64Property* StepsProperty = FindFProperty<FInt64Property>(
        FTapAchievementResult::StaticStruct(), GET_MEMBER_NAME_CHECKED(FTapAchievementResult, CurrentSteps));
    if (TestNotNull(TEXT("Blueprint CurrentSteps is int64"), StepsProperty))
    {
        TestTrue(TEXT("CurrentSteps remains Blueprint readable"), StepsProperty->HasAllPropertyFlags(CPF_BlueprintVisible | CPF_BlueprintReadOnly));
        FTapAchievementResult Result;
        StepsProperty->SetPropertyValue_InContainer(&Result, MAX_int64);
        TestEqual(TEXT("Reflection preserves int64"), Result.CurrentSteps, MAX_int64);
    }
#if (PLATFORM_WINDOWS || PLATFORM_MAC) && !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    TestNotNull(TEXT("Dedicated debug console command is registered"),
        IConsoleManager::Get().FindConsoleObject(TEXT("TapAchievement.DebugShowToast")));
#endif
    return true;
}

#endif

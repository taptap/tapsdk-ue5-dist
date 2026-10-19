// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC)

#include "Desktop/TapAchievementDesktop.h"
#include "TapAchievementCallbackManager.h"
#include "Dom/JsonObject.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementIncrementUnlockTest,
    "TapSDK.Achievement.Desktop.IncrementUnlockCallbacks",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementIncrementUnlockTest::RunTest(const FString&)
{
    FTapAchievementDesktop Desktop;
    Desktop.SetToastEnable(false);
    TArray<int32> Codes;
    TArray<FTapAchievementResult> Results;
    const FDelegateHandle Handle = FTapAchievementCallbackManager::Get().TestSuccess.AddLambda(
        [&](int32 Code, const FTapAchievementResult& Result)
        {
            Codes.Add(Code);
            Results.Add(Result);
        });
    ON_SCOPE_EXIT { FTapAchievementCallbackManager::Get().TestSuccess.Remove(Handle); };

    const auto Normal = MakeShared<FJsonObject>();
    Normal->SetStringField(TEXT("id"), TEXT("normal"));
    Normal->SetStringField(TEXT("name"), TEXT("Normal achievement"));
    Normal->SetNumberField(TEXT("current_steps"), 10);
    const auto Data = MakeShared<FJsonObject>();
    Data->SetObjectField(TEXT("achievement"), Normal);
    const auto Callback = Desktop.CreateIncrementCallback(TEXT("normal"), 1, TEXT("session"), TEXT("user"), false);
    auto Complete = [&]
    {
        Codes.Reset();
        Results.Reset();
        Callback.Execute(FTapHttpJsonResult::Success(Data));
    };
    auto CheckEvent = [&](int32 Index, int32 Code, const FString& Id, ETapAchievementType Type)
    {
        if (!Codes.IsValidIndex(Index) || !Results.IsValidIndex(Index))
        {
            AddError(FString::Printf(TEXT("Missing success event at index %d"), Index));
            return;
        }
        TestEqual(TEXT("Success code order"), Codes[Index], Code);
        TestEqual(TEXT("Success achievement ID"), Results[Index].AchievementId, Id);
        TestEqual(TEXT("Success achievement type"), Results[Index].AchievementType, Type);
    };

    // Missing/false newly_unlocked means progress only, even at the target step count.
    Complete();
    TestEqual(TEXT("Missing unlock flag reports progress only"), Codes.Num(), 1);
    CheckEvent(0, ETapAchievementError::IncrementSuccess, TEXT("normal"), ETapAchievementType::Normal);
    Normal->SetBoolField(TEXT("newly_unlocked"), false);
    Complete();
    TestEqual(TEXT("No new unlock reports progress only"), Codes.Num(), 1);
    CheckEvent(0, ETapAchievementError::IncrementSuccess, TEXT("normal"), ETapAchievementType::Normal);

    // Unity emits progress first, then unlock, independently of the toast setting.
    Normal->SetBoolField(TEXT("newly_unlocked"), true);
    Complete();
    TestEqual(TEXT("New normal unlock reports both events with toast disabled"), Codes.Num(), 2);
    CheckEvent(0, ETapAchievementError::IncrementSuccess, TEXT("normal"), ETapAchievementType::Normal);
    CheckEvent(1, ETapAchievementError::UnlockSuccess, TEXT("normal"), ETapAchievementType::Normal);
    for (const auto& Result : Results)
    {
        TestEqual(TEXT("Both events carry the name"), Result.AchievementName, FString(TEXT("Normal achievement")));
        TestEqual(TEXT("Both events carry the updated steps"), Result.CurrentSteps, int64(10));
    }

    const auto Platinum = MakeShared<FJsonObject>();
    Platinum->SetStringField(TEXT("id"), TEXT("platinum"));
    Platinum->SetStringField(TEXT("name"), TEXT("Platinum achievement"));
    Platinum->SetBoolField(TEXT("newly_unlocked"), true);
    Data->SetObjectField(TEXT("platinum_achievement"), Platinum);
    Complete();
    TestEqual(TEXT("Normal and platinum unlocks produce exactly three events"), Codes.Num(), 3);
    CheckEvent(0, ETapAchievementError::IncrementSuccess, TEXT("normal"), ETapAchievementType::Normal);
    CheckEvent(1, ETapAchievementError::UnlockSuccess, TEXT("normal"), ETapAchievementType::Normal);
    CheckEvent(2, ETapAchievementError::UnlockSuccess, TEXT("platinum"), ETapAchievementType::Platinum);

    Normal->SetBoolField(TEXT("newly_unlocked"), false);
    Complete();
    TestEqual(TEXT("Platinum unlock does not imply a new normal unlock"), Codes.Num(), 2);
    CheckEvent(0, ETapAchievementError::IncrementSuccess, TEXT("normal"), ETapAchievementType::Normal);
    CheckEvent(1, ETapAchievementError::UnlockSuccess, TEXT("platinum"), ETapAchievementType::Platinum);
    return true;
}

#endif

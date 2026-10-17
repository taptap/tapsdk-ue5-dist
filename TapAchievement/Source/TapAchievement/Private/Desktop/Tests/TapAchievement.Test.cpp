// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC)

#include "TapAchievement.h"
#include "TapAchievementCallbackManager.h"
#include "TapAchievementTypes.h"
#include "TapAchievementToastWidget.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Desktop/TapAchievementDesktop.h"
#include "UObject/EnumProperty.h"
#include "UObject/UnrealType.h"

// ── Test 1: Unlock 未初始化 → OnAchievementFailure(80000) 同步触发 ────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementUnlockNotInitTest,
    "TapSDK.Achievement.UTapAchievement.UnlockBeforeInit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementUnlockNotInitTest::RunTest(const FString&)
{
    AddExpectedError(TEXT("当前应用还未初始化"), EAutomationExpectedErrorFlags::Contains, 1);
    int32  GotCode = 0;
    FString GotId;
    FDelegateHandle H = FTapAchievementCallbackManager::Get().TestFailure.AddLambda(
        [&](const FString& Id, int32 Code, const FString&)
        { GotId = Id; GotCode = Code; });

    UTapAchievement::Unlock(TEXT("ach_001"));

    FTapAchievementCallbackManager::Get().TestFailure.Remove(H);

    TestEqual(TEXT("AchievementId"), GotId, FString(TEXT("ach_001")));
    TestEqual(TEXT("ErrorCode NotInitialized"), GotCode,
              ETapAchievementError::NotInitialized);
    return true;
}

// ── Test 2: Increment Step=0 → OnAchievementFailure 触发 ─────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementIncrementZeroStepTest,
    "TapSDK.Achievement.UTapAchievement.IncrementZeroStep",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementIncrementZeroStepTest::RunTest(const FString&)
{
    AddExpectedError(TEXT("当前应用还未初始化"), EAutomationExpectedErrorFlags::Contains, 1);
    int32 GotCode = 0;
    FDelegateHandle H = FTapAchievementCallbackManager::Get().TestFailure.AddLambda(
        [&](const FString&, int32 Code, const FString&) { GotCode = Code; });

    UTapAchievement::Increment(TEXT("ach_002"), 0);

    FTapAchievementCallbackManager::Get().TestFailure.Remove(H);

    TestTrue(TEXT("Error code fired"), GotCode != 0);
    return true;
}

// ── Test 3: OnAchievementSuccess 委托绑定正确 ─────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementSuccessBindingTest,
    "TapSDK.Achievement.UTapAchievement.SuccessBinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementSuccessBindingTest::RunTest(const FString&)
{
    int32 GotCode = 0;
    FTapAchievementResult GotResult;
    FDelegateHandle H = FTapAchievementCallbackManager::Get().TestSuccess.AddLambda(
        [&](int32 Code, const FTapAchievementResult& R)
        { GotCode = Code; GotResult = R; });

    FTapAchievementResult Fake;
    Fake.AchievementId   = TEXT("ach_plat");
    Fake.AchievementName = TEXT("Platinum");
    Fake.AchievementType = ETapAchievementType::Platinum;
    Fake.CurrentSteps    = MAX_int64;

    FTapAchievementCallbackManager::Get().Success(ETapAchievementError::UnlockSuccess, Fake);

    FTapAchievementCallbackManager::Get().TestSuccess.Remove(H);

    TestEqual(TEXT("Code"),          GotCode,                 ETapAchievementError::UnlockSuccess);
    TestEqual(TEXT("AchievementId"), GotResult.AchievementId, FString(TEXT("ach_plat")));
    TestEqual(TEXT("Type"),          GotResult.AchievementType, ETapAchievementType::Platinum);
    TestEqual(TEXT("CurrentSteps preserves int64"), GotResult.CurrentSteps, MAX_int64);
    return true;
}

// ── Test 4: Unlock 空 ID → OnAchievementFailure 触发 ─────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementUnlockEmptyIdTest,
    "TapSDK.Achievement.UTapAchievement.UnlockEmptyId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementUnlockEmptyIdTest::RunTest(const FString&)
{
    AddExpectedError(TEXT("当前应用还未初始化"), EAutomationExpectedErrorFlags::Contains, 1);
    bool bFired = false;
    FDelegateHandle H = FTapAchievementCallbackManager::Get().TestFailure.AddLambda(
        [&](const FString&, int32, const FString&) { bFired = true; });

    UTapAchievement::Unlock(TEXT(""));

    FTapAchievementCallbackManager::Get().TestFailure.Remove(H);

    TestTrue(TEXT("Failure fired for empty ID"), bFired);
    return true;
}

// ── Test 5: 离线 Unlock 同 ID 去重、不同 ID 保留 ──────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementPendingUnlockQueueTest,
    "TapSDK.Achievement.PendingQueue.UnlockSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementPendingUnlockQueueTest::RunTest(const FString&)
{
    FTapAchievementPendingQueue Queue;

    FTapAchievementPendingItem First;
    First.AchievementId = TEXT("ach_001");
    First.Type = 1;
    First.UUID = TEXT("unlock_1");
    First.UserId = TEXT("user_a");
    Queue.Enqueue(First);

    FTapAchievementPendingItem Duplicate = First;
    Duplicate.UUID = TEXT("unlock_2");
    Queue.Enqueue(Duplicate);

    FTapAchievementPendingItem Other = First;
    Other.AchievementId = TEXT("ach_002");
    Other.UUID = TEXT("unlock_3");
    Queue.Enqueue(Other);

    TestEqual(TEXT("Same unlock ID is deduplicated"), Queue.Num(), 2);

    TArray<FTapAchievementPendingItem> Replay = Queue.BeginReplay(TEXT("user_a"));
    TestEqual(TEXT("Both unique unlocks replay"), Replay.Num(), 2);
    Queue.FinishReplay(TEXT("unlock_2"), false);
    TestEqual(TEXT("Network failure keeps replayed unlock"), Queue.Num(), 2);
    TestEqual(TEXT("Retained unlock can replay again"), Queue.BeginReplay(TEXT("user_a")).Num(), 1);
    return true;
}

// ── Test 6: 离线 Increment 每次调用均保留 ────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementPendingIncrementQueueTest,
    "TapSDK.Achievement.PendingQueue.IncrementSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementPendingIncrementQueueTest::RunTest(const FString&)
{
    FTapAchievementPendingQueue Queue;
    for (int32 Index = 0; Index < 3; ++Index)
    {
        FTapAchievementPendingItem Item;
        Item.AchievementId = TEXT("ach_steps");
        Item.Steps = 1;
        Item.Type = 0;
        Item.UUID = FString::Printf(TEXT("increment_%d"), Index);
        Item.UserId = TEXT("user_a");
        Queue.Enqueue(Item);
    }

    TestEqual(TEXT("Every increment call is retained"), Queue.Num(), 3);
    TestEqual(TEXT("Every increment call replays"), Queue.BeginReplay(TEXT("user_a")).Num(), 3);
    return true;
}

// ── Test 7: 长成就名启用多行换行，Toast 高度由内容驱动 ──────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementToastLongNameLayoutTest,
    "TapSDK.Achievement.Toast.LongNameLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementToastLongNameLayoutTest::RunTest(const FString&)
{
    static const TCHAR* ToastWidgetClassPath =
        TEXT("/TapAchievement/UI/WBP_TapAchievementToast.WBP_TapAchievementToast_C");
    UClass* ToastWidgetClass = LoadClass<UTapAchievementToastWidget>(nullptr, ToastWidgetClassPath);
    if (!TestNotNull(TEXT("Toast widget class loads"), ToastWidgetClass))
    {
        return false;
    }

    UWorld* World = nullptr;
    if (GEngine)
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.World())
            {
                World = Context.World();
                break;
            }
        }
    }
    if (!TestNotNull(TEXT("Automation world exists"), World))
    {
        return false;
    }

    UTapAchievementToastWidget* ToastWidget = Cast<UTapAchievementToastWidget>(
        UUserWidget::CreateWidgetInstance(*World, ToastWidgetClass, TEXT("LongNameToastTest")));
    if (!TestNotNull(TEXT("Toast widget instance is created"), ToastWidget))
    {
        return false;
    }
    ToastWidget->TakeWidget();

    FTapAchievementResult Result;
    Result.AchievementName =
        TEXT("完成所有隐藏挑战并收集全部纪念品的传奇探险家成就");
    ToastWidget->SetAchievementData(Result);

    UTextBlock* AchievementNameText = ToastWidget->WidgetTree->FindWidget<UTextBlock>(
        TEXT("AchievementNameText"));
    if (!TestNotNull(TEXT("Achievement name text is bound"), AchievementNameText))
    {
        return false;
    }

    TestTrue(TEXT("Long achievement names wrap"), AchievementNameText->GetAutoWrapText());
    TestEqual(TEXT("Achievement name wrap width"), AchievementNameText->GetWrapTextAt(), 289.f);
    TestEqual(TEXT("Achievement name does not use ellipsis"),
        AchievementNameText->GetTextOverflowPolicy(), ETextOverflowPolicy::Clip);

    const FEnumProperty* WrappingPolicyProperty = FindFProperty<FEnumProperty>(
        UTextBlock::StaticClass(), TEXT("WrappingPolicy"));
    if (TestNotNull(TEXT("Wrapping policy property exists"), WrappingPolicyProperty))
    {
        const void* WrappingPolicyAddress = WrappingPolicyProperty->ContainerPtrToValuePtr<void>(
            AchievementNameText);
        const ETextWrappingPolicy WrappingPolicy = static_cast<ETextWrappingPolicy>(
            WrappingPolicyProperty->GetUnderlyingProperty()->GetUnsignedIntPropertyValue(
                WrappingPolicyAddress));
        TestEqual(TEXT("Unbroken names may wrap per character"), WrappingPolicy,
            ETextWrappingPolicy::AllowPerCharacterWrapping);
    }

    UWidget* ToastRoot = ToastWidget->WidgetTree->FindWidget(TEXT("ToastRoot"));
    if (TestNotNull(TEXT("Toast root exists"), ToastRoot))
    {
        UCanvasPanelSlot* ToastSlot = Cast<UCanvasPanelSlot>(ToastRoot->Slot);
        if (TestNotNull(TEXT("Toast root uses a canvas slot"), ToastSlot))
        {
            TestTrue(TEXT("Toast root height follows wrapped content"), ToastSlot->GetAutoSize());
        }
    }

    ToastWidget->RemoveFromParent();
    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC)

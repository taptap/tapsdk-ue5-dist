// Copyright TapTap. All Rights Reserved.
#include "CoreMinimal.h"

#if (PLATFORM_WINDOWS || PLATFORM_MAC) && !UE_BUILD_SHIPPING && !UE_BUILD_TEST

#include "Desktop/TapAchievementToastManager.h"
#include "HAL/IConsoleManager.h"
#include "TapAchievementModule.h"

namespace
{
    FAutoConsoleCommand DebugShowToastCommand(
        TEXT("TapAchievement.DebugShowToast"),
        TEXT("Preview a toast: TapAchievement.DebugShowToast \"Achievement name\" [Normal|Platinum]"),
        FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
        {
            if (Args.Num() < 1 || Args.Num() > 2 ||
                (Args.Num() == 2 && !Args[1].Equals(TEXT("Normal"), ESearchCase::IgnoreCase) &&
                                   !Args[1].Equals(TEXT("Platinum"), ESearchCase::IgnoreCase)))
            {
                UE_LOG(LogTapAchievement, Warning,
                    TEXT("Usage: TapAchievement.DebugShowToast \"Achievement name\" [Normal|Platinum]"));
                return;
            }

            FTapAchievementResult Result;
            Result.AchievementId = TEXT("debug_preview");
            Result.AchievementName = Args[0];
            Result.AchievementType = Args.Num() == 2 && Args[1].Equals(TEXT("Platinum"), ESearchCase::IgnoreCase)
                ? ETapAchievementType::Platinum : ETapAchievementType::Normal;
            FTapAchievementToastManager::Get().Enqueue(Result);
        }));
}

#endif

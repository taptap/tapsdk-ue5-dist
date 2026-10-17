// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC

#include "TapAchievementTypes.h"
#include "Engine/StreamableManager.h"

class UTapAchievementToastWidget;

class FTapAchievementToastManager
{
public:
    static FTapAchievementToastManager& Get();

    void Enqueue(const FTapAchievementResult& Result);
    void OnToastFinished();

private:
    TQueue<FTapAchievementResult> ToastQueue;
    bool bIsShowing = false;
    void ShowNext();

    TSharedPtr<FStreamableHandle>             WidgetHandle;
    TWeakObjectPtr<UTapAchievementToastWidget> CurrentWidget;

    static const FSoftObjectPath ToastWidgetPath;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC

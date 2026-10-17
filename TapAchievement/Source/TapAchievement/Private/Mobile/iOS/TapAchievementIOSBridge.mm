// Copyright TapTap. All Rights Reserved.
#include "Mobile/iOS/TapAchievementIOSBridge.h"

#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#include "TapAchievementModule.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"

// BridgeAchievementService 来自 TapTapAchievementSDK.xcframework。
// xcframework 由原生团队单独交付；未安装时宏为 0，所有调用降级为 no-op。
#if TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE
#import "BridgeAchievementService.h"
#endif

template <typename CallbackType>
static void RunOnMainThread(CallbackType&& Callback)
{
    if ([NSThread isMainThread])
    {
        Callback();
        return;
    }

    TSharedRef<TFunction<void()>, ESPMode::ThreadSafe> CallbackRef =
        MakeShared<TFunction<void()>, ESPMode::ThreadSafe>(Forward<CallbackType>(Callback));
    dispatch_async(dispatch_get_main_queue(), ^{
        (*CallbackRef)();
    });
}

namespace
{
    FString ToFString(NSString* In)
    {
        return In ? FString(UTF8_TO_TCHAR([In UTF8String])) : FString();
    }

    FCriticalSection                             GLock;
    TapAchievementIOSBridge::FResultCallback     GCallback;
    bool                                         GInitialized = false;

#if TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE
    void DispatchNativeResult(NSString* Result)
    {
        const FString Json = ToFString(Result);
        TapAchievementIOSBridge::FResultCallback Snapshot;
        {
            FScopeLock Lock(&GLock);
            Snapshot = GCallback;
        }
        if (Snapshot)
        {
            AsyncTask(ENamedThreads::GameThread, [Snapshot, Json]() { Snapshot(Json); });
        }
    }

    void EnsureCallbackRegistered()
    {
        [BridgeAchievementService registerCallback:^(NSString* Result)
        {
            DispatchNativeResult(Result);
        }];
    }
#endif
}

namespace TapAchievementIOSBridge
{
    void Init(FResultCallback OnPersistentResult)
    {
        {
            FScopeLock Lock(&GLock);
            if (GInitialized)
            {
                return;
            }
            GInitialized = true;
            GCallback = MoveTemp(OnPersistentResult);
        }

#if TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE
        RunOnMainThread([]()
        {
            EnsureCallbackRegistered();
            UE_LOG(LogTapAchievement, Log, TEXT("[Achievement-iOS] BridgeAchievementService registered"));
        });
#else
        UE_LOG(LogTapAchievement, Warning, TEXT("[Achievement-iOS] SDK not available: TapTapAchievementSDK.xcframework missing"));
#endif
    }

    void Unlock(const FString& AchievementId)
    {
#if TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE
        const FString Id = AchievementId;
        UE_LOG(LogTapAchievement, Log, TEXT("[Achievement-iOS] Unlock requested: %s"), *Id);
        RunOnMainThread([Id]()
        {
            EnsureCallbackRegistered();
            [BridgeAchievementService achievementId:Id.GetNSString()];
        });
#else
        UE_LOG(LogTapAchievement, Warning, TEXT("[Achievement-iOS] Unlock skipped: SDK not available"));
#endif
    }

    void Increment(const FString& AchievementId, int32 Steps)
    {
#if TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE
        const FString Id = AchievementId;
        UE_LOG(LogTapAchievement, Log, TEXT("[Achievement-iOS] Increment requested: %s, steps=%d"), *Id, Steps);
        RunOnMainThread([Id, Steps]()
        {
            EnsureCallbackRegistered();
            [BridgeAchievementService achievementId:Id.GetNSString() steps:@(Steps)];
        });
#else
        UE_LOG(LogTapAchievement, Warning, TEXT("[Achievement-iOS] Increment skipped: SDK not available"));
#endif
    }

    void ShowAchievements()
    {
#if TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE
        RunOnMainThread([]()
        {
            [BridgeAchievementService showAchievements];
        });
#else
        UE_LOG(LogTapAchievement, Warning, TEXT("[Achievement-iOS] ShowAchievements skipped: SDK not available"));
#endif
    }

    void SetToastEnable(bool bEnable)
    {
#if TAP_ACHIEVEMENT_IOS_SDK_AVAILABLE
        RunOnMainThread([bEnable]()
        {
            [BridgeAchievementService enableToast:@(bEnable)];
        });
#else
        UE_LOG(LogTapAchievement, Warning, TEXT("[Achievement-iOS] SetToastEnable skipped: SDK not available"));
#endif
    }
}

#endif // PLATFORM_IOS

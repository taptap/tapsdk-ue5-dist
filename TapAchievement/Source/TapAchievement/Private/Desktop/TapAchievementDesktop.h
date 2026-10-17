// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC

#include "TapAchievementTypes.h"
#include "Desktop/TapAchievementPendingItem.h"
#include "TapTapSdkOptions.h"
#include "Internal/Platform/TapOpenlogTracker.h"
#include "Internal/Http/TapHttpResult.h"

class FTapHttp;

/** PC 离线队列状态：保留重放中的条目，直到请求得到确定结果。 */
class FTapAchievementPendingQueue
{
public:
    bool Enqueue(const FTapAchievementPendingItem& Item);
    void AddLoaded(const FTapAchievementPendingItem& Item);
    TArray<FTapAchievementPendingItem> BeginReplay(const FString& UserId);
    void FinishReplay(const FString& UUID, bool bRemoveItem);

    int32 Num() const { return Items.Num(); }
    const TArray<FTapAchievementPendingItem>& GetItems() const { return Items; }

private:
    TArray<FTapAchievementPendingItem> Items;
    TSet<FString> UploadingUUIDs;
};

class FTapAchievementDesktop
{
public:
    static FTapAchievementDesktop& Get();

    void Initialize(const FTapTapSdkOptions& Config);

    void Unlock(const FString& AchievementId);
    void Increment(const FString& AchievementId, int32 Step);
    void ShowAchievements();
    void SetToastEnable(bool bEnable);

private:
    bool CheckPreconditions(const FString& AchievementId, FString& OutUserId);
    bool CanSendForUser(const FString& UserId) const;

    TSharedPtr<FTapHttp> HttpClient;

    void DoUnlock(const FString& AchievementId, const FString& SessionId, const FString& UserId, bool bFromQueue);
    void DoIncrement(const FString& AchievementId, int32 Step, const FString& SessionId, const FString& UserId, bool bFromQueue);
    FTapHttpJsonResultDelegate CreateUnlockCallback(const FString& AchievementId, const FString& SessionId, const FString& UserId, bool bFromQueue);
    FTapHttpJsonResultDelegate CreateIncrementCallback(const FString& AchievementId, int32 Step, const FString& SessionId, const FString& UserId, bool bFromQueue);

    FTapAchievementPendingQueue PendingQueue;
    void EnqueuePending(const FTapAchievementPendingItem& Item);
    void FinishPendingReplay(const FString& UUID, bool bRemoveItem);
    void FlushPendingQueue();
    void SaveQueueToDisk() const;
    void LoadQueueFromDisk();
    FString GetQueueFilePath() const;

    bool bToastEnabled = true;
    bool bInitialized = false;

#if WITH_DEV_AUTOMATION_TESTS
    friend class FTapAchievementPendingAccountTest;
    friend class FTapAchievementIncrementUnlockTest;
    FString QueueFilePathForTest;
#endif

    FTapOpenlogTracker Tracker{TEXT("TapAchievement"), TEXT("")};
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC

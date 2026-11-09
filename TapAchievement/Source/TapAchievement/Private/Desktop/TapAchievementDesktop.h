// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC

#include "TapAchievementTypes.h"
#include "Desktop/TapAchievementPendingItem.h"
#include "TapTapSdkOptions.h"
#include "TapCore.h"
#include "Internal/Platform/TapOpenlogTracker.h"
#include "Internal/Http/TapHttpResult.h"

class FTapHttp;

/** PC 离线队列状态：保留重放中的条目，直到请求得到确定结果。 */
class FTapAchievementPendingQueue
{
public:
    bool Enqueue(const FTapAchievementPendingItem& Item);
    TArray<FTapAchievementPendingItem> BeginReplay(const FString& UserId);
    void FinishReplay(const FString& UUID, bool bRemoveItem);
    void ReplaceForUser(const FString& UserId, const TArray<FTapAchievementPendingItem>& LoadedItems);

    int32 Num() const { return Items.Num(); }
    const TArray<FTapAchievementPendingItem>& GetItems() const { return Items; }

private:
    TArray<FTapAchievementPendingItem> Items;
    TSet<FString> UploadingUUIDs;
};

class FTapAchievementDesktop : private ITapInitCallback
{
public:
    static FTapAchievementDesktop& Get();
    ~FTapAchievementDesktop() override;

    void Initialize(const FTapTapSdkOptions& Config);

    void Unlock(const FString& AchievementId);
    void Increment(const FString& AchievementId, int32 Step);
    void ShowAchievements();
    void SetToastEnable(bool bEnable);

private:
    void OnInitSuccess() override;
    void OnInitFail(int32 ErrorCode, const FString& ErrorMessage) override;
    bool CheckPreconditions(const FString& AchievementId, FString& OutUserId);
    bool CanSendForUser(const FString& UserId) const;

    TSharedPtr<FTapHttp> HttpClient;

    void DoUnlock(const FString& AchievementId, const FString& SessionId, const FString& UserId, bool bFromQueue);
    void DoIncrement(const FString& AchievementId, int32 Step, const FString& SessionId, const FString& UserId, bool bFromQueue);
    FTapHttpJsonResultDelegate CreateUnlockCallback(const FString& AchievementId, const FString& SessionId, const FString& UserId, bool bFromQueue);
    FTapHttpJsonResultDelegate CreateIncrementCallback(const FString& AchievementId, int32 Step, const FString& SessionId, const FString& UserId, bool bFromQueue);

    FTapAchievementPendingQueue PendingQueue;
    void EnqueuePending(const FTapAchievementPendingItem& Item);
    void FinishPendingReplay(const FString& UUID, const FString& UserId, bool bRemoveItem);
    void FlushPendingQueue();
    void SaveQueueToDisk();
    /** Hold one file lease across read, delta application and publication, then refresh this account. */
    bool LoadQueueFromDisk(const FString& UserId);
    FString GetQueueFilePath(const FString& UserId) const;
    FString GetLegacyQueueFilePath() const;

    FString ClientId;
    // Only uncommitted changes may be merged into the latest disk state, never an old snapshot.
    TMap<FString, FTapAchievementPendingItem> PendingAdds;
    TMap<FString, FString> PendingRemovals; // UUID -> request-time account

    bool bToastEnabled = true;
    bool bInitialized = false;
    bool bQueueNeedsSave = false;
    bool bQueueReady = false;
    FTSTicker::FDelegateHandle CacheRetryTicker;

#if WITH_DEV_AUTOMATION_TESTS
    friend class FTapAchievementPendingAccountTest;
    friend class FTapAchievementIncrementUnlockTest;
    friend class FTapAchievementSharedCacheTest;
    friend class FTapAchievementAccountPersistenceTest;
    FString QueueRootForTest;
#endif

    FTapOpenlogTracker Tracker{TEXT("TapAchievement"), TEXT("")};
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC

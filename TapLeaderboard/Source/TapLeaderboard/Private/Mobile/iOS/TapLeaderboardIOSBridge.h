// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_IOS

/**
 * iOS 桥接：直接调 BridgeLeaderboardService ObjC 类方法。
 * 所有异步结果经统一 GCallback 回到 GameThread，op 字段区分操作类型。
 * 与 TapAchievementIOSBridge 同构。
 */
namespace TapLeaderboardIOSBridge
{
    using FResultCallback = TFunction<void(const FString& /*NormalizedJson*/)>;

    /** 注册统一回调（调用一次）。所有操作结果均触发此 callback。 */
    void Init(FResultCallback OnResult);

    // Fire-and-forget
    void OpenLeaderboard(const FString& LeaderboardId, const FString& Collection);
    void OpenUserProfile(const FString& OpenId, const FString& UnionId);

    // 异步（结果通过 GCallback 返回）
    void SubmitScores(const FString& ScoresJson);
    void LoadLeaderboardScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& NextPage, const FString& PeriodToken);
    void LoadCurrentPlayerLeaderboardScore(
        const FString& LeaderboardId, const FString& Collection,
        const FString& PeriodToken);
    void LoadPlayerCenteredScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& PeriodToken, int32 MaxCount);

    // 持久回调
    void RegisterLeaderboardCallback();
    void UnregisterLeaderboardCallback();
    void SetShareCallback();
}

#endif // PLATFORM_IOS

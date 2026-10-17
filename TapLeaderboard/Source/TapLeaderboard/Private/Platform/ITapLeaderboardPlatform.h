// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapLeaderboardTypes.h"

/**
 * 排行榜平台抽象接口（移动端专属：Android / iOS）。
 * 所有异步结果经 UTapLeaderboard 静态委托广播给调用方。
 */
class TAPLEADERBOARD_API ITapLeaderboardPlatform
{
public:
    virtual ~ITapLeaderboardPlatform() = default;

    virtual void OpenLeaderboard(const FString& LeaderboardId, const FString& Collection) = 0;
    virtual void OpenUserProfile(const FString& OpenId, const FString& UnionId) = 0;
    virtual void SubmitScores(const TArray<FTapLeaderboardScoreItem>& Scores) = 0;
    virtual void LoadLeaderboardScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& NextPage, const FString& PeriodToken) = 0;
    virtual void LoadCurrentPlayerLeaderboardScore(
        const FString& LeaderboardId, const FString& Collection,
        const FString& PeriodToken) = 0;
    virtual void LoadPlayerCenteredScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& PeriodToken, int32 MaxCount) = 0;
    virtual void RegisterLeaderboardCallback() = 0;
    virtual void UnregisterLeaderboardCallback() = 0;
    virtual void SetShareCallback() = 0;
    virtual FString GetPlatformName() const = 0;
};

/** 按 #if PLATFORM_* 创建并缓存单例平台实现。 */
class TAPLEADERBOARD_API FTapLeaderboardPlatformFactory
{
public:
    static TSharedPtr<ITapLeaderboardPlatform> CreatePlatform();
    static void Reset();

private:
    static TSharedPtr<ITapLeaderboardPlatform> PlatformInstance;
};

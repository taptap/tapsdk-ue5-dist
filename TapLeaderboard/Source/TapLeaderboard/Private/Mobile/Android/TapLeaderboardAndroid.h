// Copyright TapTap. All Rights Reserved.
#pragma once

#if PLATFORM_ANDROID

#include "CoreMinimal.h"
#include "Platform/ITapLeaderboardPlatform.h"

/**
 * Android 实现：经专属 routeToTapLeaderboard → TapLeaderboardUERouter 调原生 SDK。
 * 所有结果（同步 fire-and-forget 及异步 per-call）通过 JNI 持久回调
 * nativeOnLeaderboardResult(String resultJson) 返回。
 */
class FTapLeaderboardAndroid : public ITapLeaderboardPlatform
{
public:
    FTapLeaderboardAndroid();

    virtual void OpenLeaderboard(const FString& LeaderboardId, const FString& Collection) override;
    virtual void OpenUserProfile(const FString& OpenId, const FString& UnionId) override;
    virtual void SubmitScores(const TArray<FTapLeaderboardScoreItem>& Scores) override;
    virtual void LoadLeaderboardScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& NextPage, const FString& PeriodToken) override;
    virtual void LoadCurrentPlayerLeaderboardScore(
        const FString& LeaderboardId, const FString& Collection,
        const FString& PeriodToken) override;
    virtual void LoadPlayerCenteredScores(
        const FString& LeaderboardId, const FString& Collection,
        const FString& PeriodToken, int32 MaxCount) override;
    virtual void RegisterLeaderboardCallback() override;
    virtual void UnregisterLeaderboardCallback() override;
    virtual void SetShareCallback() override;
    virtual FString GetPlatformName() const override { return TEXT("Android"); }

    /** JNI 持久回调入口（由 TapLeaderboardUERouter.nativeOnLeaderboardResult 触发）*/
    static void DispatchResult(const FString& ResultJson);

private:
    static FString CallRoute(const FString& Method, const FString& ParamsJson);
    static FString BuildRequest(const FString& Method, const FString& ParamsJson);
};

#endif // PLATFORM_ANDROID

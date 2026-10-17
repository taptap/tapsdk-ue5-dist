// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_IOS

#include "Platform/ITapLeaderboardPlatform.h"

class FTapLeaderboardiOS : public ITapLeaderboardPlatform
{
public:
    FTapLeaderboardiOS();

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
    virtual FString GetPlatformName() const override { return TEXT("iOS"); }
};

#endif // PLATFORM_IOS

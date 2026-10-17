// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapLeaderboardTypes.generated.h"

// ── 提交分数请求项 ─────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapLeaderboardScoreItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite) FString LeaderboardId;
    UPROPERTY(BlueprintReadWrite) int64   Score = 0;
};

// ── 用户信息 ──────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapLeaderboardUser
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString OpenId;
    UPROPERTY(BlueprintReadOnly) FString UnionId;
    UPROPERTY(BlueprintReadOnly) FString Name;
    UPROPERTY(BlueprintReadOnly) FString AvatarUrl;
};

// ── 单条分数 ──────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapLeaderboardScore
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) int64                 Rank  = 0;
    UPROPERTY(BlueprintReadOnly) FString               RankDisplay;
    UPROPERTY(BlueprintReadOnly) int64                 Score = 0;
    UPROPERTY(BlueprintReadOnly) FString               ScoreDisplay;
    UPROPERTY(BlueprintReadOnly) int64                 ScoreSubmittedTime = 0;
    UPROPERTY(BlueprintReadOnly) FTapLeaderboardUser   User;
};

// ── 排行榜周期 ────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapLeaderboardPeriod
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString Display;
    UPROPERTY(BlueprintReadOnly) FString PeriodToken;
};

// ── 排行榜基本信息 ─────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapLeaderboardInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString                         Id;
    UPROPERTY(BlueprintReadOnly) FString                         Name;
    UPROPERTY(BlueprintReadOnly) TArray<FTapLeaderboardPeriod>  AvailablePeriods;
    UPROPERTY(BlueprintReadOnly) FTapLeaderboardPeriod          Period;
    UPROPERTY(BlueprintReadOnly) FTapLeaderboardScore           Score;
};

// ── LoadLeaderboardScores / LoadPlayerCenteredScores 响应 ─────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapLeaderboardScoresResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FTapLeaderboardInfo          Leaderboard;
    UPROPERTY(BlueprintReadOnly) TArray<FTapLeaderboardScore> Scores;
    UPROPERTY(BlueprintReadOnly) FString                      NextPage;
};

// ── LoadCurrentPlayerLeaderboardScore 响应 ────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapUserScoreResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FTapLeaderboardInfo  Leaderboard;
    UPROPERTY(BlueprintReadOnly) FTapLeaderboardScore CurrentUserScore;
};

// ── SubmitScores 响应单项 ─────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapSubmitScoreResultItem
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) FString LeaderboardId;
    UPROPERTY(BlueprintReadOnly) FString OpenId;
    UPROPERTY(BlueprintReadOnly) FString UnionId;
    UPROPERTY(BlueprintReadOnly) FString PeriodToken;
    UPROPERTY(BlueprintReadOnly) bool    bNewBest      = false;
    UPROPERTY(BlueprintReadOnly) int64   RawScore      = 0;
    UPROPERTY(BlueprintReadOnly) FString ScoreDisplay;
};

// ── SubmitScores 响应 ─────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct TAPLEADERBOARD_API FTapSubmitScoresResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly) TArray<FTapSubmitScoreResultItem> Items;
};

// ── 委托（对齐 Unity ITapTapLeaderboard / callback 体系） ─────────────────────
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLeaderboardScoresLoaded,
    const FTapLeaderboardScoresResult& /*Result*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCurrentPlayerScoreLoaded,
    const FTapUserScoreResult& /*Result*/);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnScoresSubmitted,
    const FTapSubmitScoresResult& /*Result*/);

/** 所有 API 失败时广播：操作名、错误码、错误信息 */
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnLeaderboardError,
    const FString& /*Op*/, int32 /*ErrCode*/, const FString& /*ErrMsg*/);

/** registerLeaderboardCallback 持久回调结果 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLeaderboardEvent,
    int32 /*Code*/, const FString& /*Message*/);

/** setShareCallback 分享结果 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLeaderboardShareResult,
    bool /*bSuccess*/, const FString& /*DataOrError*/);

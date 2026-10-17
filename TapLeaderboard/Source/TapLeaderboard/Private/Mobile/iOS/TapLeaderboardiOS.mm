// Copyright TapTap. All Rights Reserved.
#include "Mobile/iOS/TapLeaderboardiOS.h"

#if PLATFORM_IOS

#include "Mobile/iOS/TapLeaderboardIOSBridge.h"
#include "TapLeaderboardResultParser.h"
#include "TapLeaderboardCallbackManager.h"
#include "TapLeaderboardModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// DispatchResult 与 Android 完全对称，逻辑抽到公共函数中避免重复
static void DispatchLeaderboardResult(const FString& Json)
{
    const TapLeaderboardResultParser::FParsed P = TapLeaderboardResultParser::Parse(Json);
    if (!P.bParsed) { UE_LOG(LogTapLeaderboard, Warning, TEXT("[iOS] unparsed result: %s"), *Json); return; }

    if (P.Op == TEXT("loadScores") || P.Op == TEXT("loadPlayerCenteredScores"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().Scores(P.Op,TapLeaderboardResultParser::ParseScoresResult(P.Data));
        else
            FTapLeaderboardCallbackManager::Get().Error(P.Op, P.ErrCode, P.ErrMsg);
    }
    else if (P.Op == TEXT("loadCurrentPlayerScore"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().Current(TapLeaderboardResultParser::ParseUserScoreResult(P.Data));
        else
            FTapLeaderboardCallbackManager::Get().Error(P.Op, P.ErrCode, P.ErrMsg);
    }
    else if (P.Op == TEXT("submitScores"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().Submitted(TapLeaderboardResultParser::ParseSubmitResult(P.Data));
        else
            FTapLeaderboardCallbackManager::Get().Error(P.Op, P.ErrCode, P.ErrMsg);
    }
    else if (P.Op == TEXT("leaderboardCallback"))
    {
        FTapLeaderboardCallbackManager::Get().Event(P.EventCode, P.EventMessage);
    }
    else if (P.Op == TEXT("shareCallback"))
    {
        if (P.bIsSuccess)
            FTapLeaderboardCallbackManager::Get().ShareResult(true, P.Data);
        else
            FTapLeaderboardCallbackManager::Get().ShareResult(false, P.ErrMsg);
    }
}

FTapLeaderboardiOS::FTapLeaderboardiOS()
{
    TapLeaderboardIOSBridge::Init([](const FString& Json)
    {
        // 已 marshal 到 GameThread（bridge 内部保证）
        DispatchLeaderboardResult(Json);
    });
    UE_LOG(LogTapLeaderboard, Log, TEXT("[iOS] TapLeaderboard bridge initialized"));
}

void FTapLeaderboardiOS::OpenLeaderboard(const FString& LeaderboardId, const FString& Collection)
{
    TapLeaderboardIOSBridge::OpenLeaderboard(LeaderboardId, Collection);
}

void FTapLeaderboardiOS::OpenUserProfile(const FString& OpenId, const FString& UnionId)
{
    TapLeaderboardIOSBridge::OpenUserProfile(OpenId, UnionId);
}

void FTapLeaderboardiOS::SubmitScores(const TArray<FTapLeaderboardScoreItem>& Scores)
{
    // 手动构建 JSON 数组，避免 int64 → double 精度丢失（double 仅 53 位尾数）
    FString Json = TEXT("[");
    for (int32 i = 0; i < Scores.Num(); ++i)
    {
        if (i > 0) Json += TEXT(",");
        FString EscapedId = Scores[i].LeaderboardId;
        EscapedId.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
        EscapedId.ReplaceInline(TEXT("\""), TEXT("\\\""));
        Json += FString::Printf(TEXT("{\"leaderboard_id\":\"%s\",\"score\":%lld}"),
            *EscapedId, Scores[i].Score);
    }
    Json += TEXT("]");
    TapLeaderboardIOSBridge::SubmitScores(Json);
}

void FTapLeaderboardiOS::LoadLeaderboardScores(
    const FString& LeaderboardId, const FString& Collection,
    const FString& NextPage, const FString& PeriodToken)
{
    TapLeaderboardIOSBridge::LoadLeaderboardScores(LeaderboardId, Collection, NextPage, PeriodToken);
}

void FTapLeaderboardiOS::LoadCurrentPlayerLeaderboardScore(
    const FString& LeaderboardId, const FString& Collection, const FString& PeriodToken)
{
    TapLeaderboardIOSBridge::LoadCurrentPlayerLeaderboardScore(LeaderboardId, Collection, PeriodToken);
}

void FTapLeaderboardiOS::LoadPlayerCenteredScores(
    const FString& LeaderboardId, const FString& Collection,
    const FString& PeriodToken, int32 MaxCount)
{
    TapLeaderboardIOSBridge::LoadPlayerCenteredScores(LeaderboardId, Collection, PeriodToken, MaxCount);
}

void FTapLeaderboardiOS::RegisterLeaderboardCallback()
{
    TapLeaderboardIOSBridge::RegisterLeaderboardCallback();
}

void FTapLeaderboardiOS::UnregisterLeaderboardCallback()
{
    TapLeaderboardIOSBridge::UnregisterLeaderboardCallback();
}

void FTapLeaderboardiOS::SetShareCallback()
{
    TapLeaderboardIOSBridge::SetShareCallback();
}

#endif // PLATFORM_IOS

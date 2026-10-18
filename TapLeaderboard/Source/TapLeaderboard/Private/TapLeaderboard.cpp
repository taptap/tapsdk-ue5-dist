// Copyright TapTap. All Rights Reserved.
#include "TapLeaderboard.h"
#include "TapLeaderboardModule.h"
#include "Platform/ITapLeaderboardPlatform.h"
#include "Internal/TapSdkChecker.h"
#include "TapLeaderboardCallbackManager.h"

void UTapLeaderboard::OpenLeaderboard(const FString& LeaderboardId, const FString& Collection)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
    {
        // Register the native share callback before the WebView reads IS_SHARING_ENABLED.
        P->OpenLeaderboard(LeaderboardId, Collection);
    }
}

void UTapLeaderboard::ShowTapUserProfile(const FString& OpenId, const FString& UnionId)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        P->OpenUserProfile(OpenId, UnionId);
}

void UTapLeaderboard::SubmitScores(const TArray<FTapLeaderboardScoreItem>& Scores,const FTapLeaderboardSubmitCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError)
{
    check(IsInGameThread());
    if (!FTapSdkChecker::CheckInitialize()){OnError.ExecuteIfBound(TEXT("submitScores"),-1,TEXT("TapSDK not initialized"));return;}
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        FTapLeaderboardCallbackManager::Get().EnqueueSubmit(OnSuccess, OnError, [P, Scores]() { P->SubmitScores(Scores); });
    else
        OnError.ExecuteIfBound(TEXT("submitScores"), -1, TEXT("TapLeaderboard platform unavailable"));
}

void UTapLeaderboard::LoadLeaderboardScores(
    const FString& LeaderboardId, const FString& Collection,
    const FString& NextPage, const FString& PeriodToken,const FTapLeaderboardScoresCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError)
{
    check(IsInGameThread());
    if (!FTapSdkChecker::CheckInitialize()){OnError.ExecuteIfBound(TEXT("loadScores"),-1,TEXT("TapSDK not initialized"));return;}
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        FTapLeaderboardCallbackManager::Get().EnqueueScores(TEXT("loadScores"), OnSuccess, OnError,
            [P, LeaderboardId, Collection, NextPage, PeriodToken]() { P->LoadLeaderboardScores(LeaderboardId, Collection, NextPage, PeriodToken); });
    else
        OnError.ExecuteIfBound(TEXT("loadScores"), -1, TEXT("TapLeaderboard platform unavailable"));
}

void UTapLeaderboard::LoadCurrentPlayerLeaderboardScore(
    const FString& LeaderboardId, const FString& Collection, const FString& PeriodToken,const FTapLeaderboardCurrentScoreCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError)
{
    check(IsInGameThread());
    if (!FTapSdkChecker::CheckInitialize()){OnError.ExecuteIfBound(TEXT("loadCurrentPlayerScore"),-1,TEXT("TapSDK not initialized"));return;}
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        FTapLeaderboardCallbackManager::Get().EnqueueCurrent(OnSuccess, OnError,
            [P, LeaderboardId, Collection, PeriodToken]() { P->LoadCurrentPlayerLeaderboardScore(LeaderboardId, Collection, PeriodToken); });
    else
        OnError.ExecuteIfBound(TEXT("loadCurrentPlayerScore"), -1, TEXT("TapLeaderboard platform unavailable"));
}

void UTapLeaderboard::LoadPlayerCenteredScores(
    const FString& LeaderboardId, const FString& Collection,
    const FString& PeriodToken, int32 MaxCount,const FTapLeaderboardScoresCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError)
{
    check(IsInGameThread());
    if (!FTapSdkChecker::CheckInitialize()){OnError.ExecuteIfBound(TEXT("loadPlayerCenteredScores"),-1,TEXT("TapSDK not initialized"));return;}
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        FTapLeaderboardCallbackManager::Get().EnqueueScores(TEXT("loadPlayerCenteredScores"), OnSuccess, OnError,
            [P, LeaderboardId, Collection, PeriodToken, MaxCount]() { P->LoadPlayerCenteredScores(LeaderboardId, Collection, PeriodToken, MaxCount); });
    else
        OnError.ExecuteIfBound(TEXT("loadPlayerCenteredScores"), -1, TEXT("TapLeaderboard platform unavailable"));
}

void UTapLeaderboard::RegisterLeaderboardCallback(TScriptInterface<ITapLeaderboardCallback> Callback)
{
    if(!FTapSdkChecker::CheckInitialize())return;auto&M=FTapLeaderboardCallbackManager::Get();const bool First=M.Events.Num()==0;M.Events.Register(Callback.GetObject(),UTapLeaderboardCallback::StaticClass());
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        if(First)P->RegisterLeaderboardCallback();
}

void UTapLeaderboard::UnregisterLeaderboardCallback(TScriptInterface<ITapLeaderboardCallback> Callback)
{
    if(!FTapSdkChecker::CheckInitialize())return;
    auto&M=FTapLeaderboardCallbackManager::Get();M.Events.Unregister(Callback.GetObject());
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        if(M.Events.Num()==0)P->UnregisterLeaderboardCallback();
}

void UTapLeaderboard::SetShareCallback(TScriptInterface<ITapLeaderboardShareCallback> Callback)
{
    if (!FTapSdkChecker::CheckInitialize()) return;
    auto& Manager = FTapLeaderboardCallbackManager::Get();
    Manager.SetShareCallback(Callback.GetObject());
    if (Manager.Share.Num() == 0) return;
    if (TSharedPtr<ITapLeaderboardPlatform> P = FTapLeaderboardPlatformFactory::CreatePlatform())
        P->SetShareCallback();
}

// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapLeaderboardTypes.h"
#include "TapLeaderboardCallback.h"
#include "TapLeaderboard.generated.h"
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapLeaderboardScoresCallback,const FTapLeaderboardScoresResult&,Result);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapLeaderboardCurrentScoreCallback,const FTapUserScoreResult&,Result);
DECLARE_DYNAMIC_DELEGATE_OneParam(FTapLeaderboardSubmitCallback,const FTapSubmitScoresResult&,Result);
DECLARE_DYNAMIC_DELEGATE_ThreeParams(FTapLeaderboardErrorCallback,const FString&,Operation,int32,Code,const FString&,Message);

UCLASS()
class TAPLEADERBOARD_API UTapLeaderboard : public UObject
{
    GENERATED_BODY()
public:
    // ── UI 操作（fire-and-forget，对齐 Unity OpenLeaderboard / OpenUserProfile） ──
    /** 打开排行榜，并在 WebView 创建前自动注册分享回调 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void OpenLeaderboard(const FString& LeaderboardId, const FString& Collection);

    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void OpenUserProfile(const FString& OpenId, const FString& UnionId);

    // ── 异步操作（结果通过对应委托广播，失败统一走 OnError） ──────────────────────
    /** 批量提交分数（一次最多 5 个），结果通过 OnScoresSubmitted / OnError 广播 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void SubmitScores(const TArray<FTapLeaderboardScoreItem>& Scores,const FTapLeaderboardSubmitCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError);

    /** 获取排行榜分数列表，结果通过 OnLeaderboardScoresLoaded / OnError 广播 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void LoadLeaderboardScores(
        const FString& LeaderboardId,
        const FString& Collection,
        const FString& NextPage,
        const FString& PeriodToken,const FTapLeaderboardScoresCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError);

    /** 获取当前用户排行榜分数，结果通过 OnCurrentPlayerScoreLoaded / OnError 广播 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void LoadCurrentPlayerLeaderboardScore(
        const FString& LeaderboardId,
        const FString& Collection,
        const FString& PeriodToken,const FTapLeaderboardCurrentScoreCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError);

    /** 获取当前用户附近的分数列表，结果通过 OnLeaderboardScoresLoaded / OnError 广播 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void LoadPlayerCenteredScores(
        const FString& LeaderboardId,
        const FString& Collection,
        const FString& PeriodToken,
        int32 MaxCount,const FTapLeaderboardScoresCallback& OnSuccess,const FTapLeaderboardErrorCallback& OnError);

    // ── 持久回调注册 ──────────────────────────────────────────────────────────
    /** 注册排行榜状态回调，结果通过 OnLeaderboardEvent 广播 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void RegisterLeaderboardCallback(TScriptInterface<ITapLeaderboardCallback> Callback);

    /** 取消注册排行榜状态回调 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void UnregisterLeaderboardCallback(TScriptInterface<ITapLeaderboardCallback> Callback);

    /** 替换当前分享回调；传入空接口清除监听。回调对象需由调用方持有。 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void SetShareCallback(TScriptInterface<ITapLeaderboardShareCallback> Callback);
};

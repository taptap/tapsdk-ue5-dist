// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapLeaderboardTypes.h"
#include "TapLeaderboardCallback.h"
#include "TapLeaderboard.generated.h"
DECLARE_DELEGATE_OneParam(FTapLeaderboardScoresCallback, const FTapLeaderboardScoresResult&);
DECLARE_DELEGATE_OneParam(FTapLeaderboardCurrentScoreCallback, const FTapUserScoreResult&);
DECLARE_DELEGATE_OneParam(FTapLeaderboardSubmitCallback, const FTapSubmitScoresResult&);
DECLARE_DELEGATE_ThreeParams(FTapLeaderboardErrorCallback, const FString&, int32, const FString&);

UCLASS()
class TAPLEADERBOARD_API UTapLeaderboard : public UObject
{
    GENERATED_BODY()
public:
    // ── UI 操作（fire-and-forget，对齐 Unity OpenLeaderboard / ShowTapUserProfile） ──
    /** 打开排行榜，并在 WebView 创建前自动注册分享回调 */
    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void OpenLeaderboard(const FString& LeaderboardId, const FString& Collection);

    UFUNCTION(BlueprintCallable, Category="TapLeaderboard")
    static void ShowTapUserProfile(const FString& OpenId, const FString& UnionId);

    // C++ 单次请求：在 Game Thread 调用，SDK 保存 Delegate 副本到完成或模块关闭。
    // 提前失败可能同步回调；模块关闭清空待处理请求，不再通知。
    // CreateUObject / CreateWeakLambda 为弱绑定，不会为接收对象保活。
    /** 批量提交分数（一次最多 5 个） */
    static void SubmitScores(const TArray<FTapLeaderboardScoreItem>& Scores,
        const FTapLeaderboardSubmitCallback& OnSuccess,
        const FTapLeaderboardErrorCallback& OnError);

    /** 获取排行榜分数列表 */
    static void LoadLeaderboardScores(
        const FString& LeaderboardId,
        const FString& Collection,
        const FString& NextPage,
        const FString& PeriodToken,
        const FTapLeaderboardScoresCallback& OnSuccess,
        const FTapLeaderboardErrorCallback& OnError);

    /** 获取当前用户排行榜分数 */
    static void LoadCurrentPlayerLeaderboardScore(
        const FString& LeaderboardId,
        const FString& Collection,
        const FString& PeriodToken,
        const FTapLeaderboardCurrentScoreCallback& OnSuccess,
        const FTapLeaderboardErrorCallback& OnError);

    /** 获取当前用户附近的分数列表 */
    static void LoadPlayerCenteredScores(
        const FString& LeaderboardId,
        const FString& Collection,
        const FString& PeriodToken,
        int32 MaxCount,
        const FTapLeaderboardScoresCallback& OnSuccess,
        const FTapLeaderboardErrorCallback& OnError);

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

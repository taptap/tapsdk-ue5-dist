#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapLeaderboardCallback.h"
#include "TapLeaderboard.h"
class FTapLeaderboardCallbackManager
{
public:
    static FTapLeaderboardCallbackManager& Get();
    FTapWeakCallbackRegistry Events, Share;
    void Reset();
    void SetShareCallback(UObject* Callback);
    // Current UE bridge responses carry an operation name, not a request ID. Only one
    // request per operation may be in flight, otherwise callbacks can be swapped.
    void EnqueueScores(const FString& Operation, const FTapLeaderboardScoresCallback& S,
        const FTapLeaderboardErrorCallback& E, TFunction<void()> Dispatch);
    void EnqueueCurrent(const FTapLeaderboardCurrentScoreCallback& S,
        const FTapLeaderboardErrorCallback& E, TFunction<void()> Dispatch);
    void EnqueueSubmit(const FTapLeaderboardSubmitCallback& S,
        const FTapLeaderboardErrorCallback& E, TFunction<void()> Dispatch);
    void Scores(const FString& Op, const FTapLeaderboardScoresResult& R);
    void Current(const FTapUserScoreResult& R);
    void Submitted(const FTapSubmitScoresResult& R);
    void Error(const FString& Op, int32 C, const FString& M);
    void Event(int32 C, const FString& M);
    void ShareResult(bool B, const FString& D);

private:
    struct FScores { FString Operation; FTapLeaderboardScoresCallback S; FTapLeaderboardErrorCallback E; TFunction<void()> Dispatch; };
    struct FCurrent { FTapLeaderboardCurrentScoreCallback S; FTapLeaderboardErrorCallback E; TFunction<void()> Dispatch; };
    struct FSubmit { FTapLeaderboardSubmitCallback S; FTapLeaderboardErrorCallback E; TFunction<void()> Dispatch; };
    FCriticalSection Lock;
    TArray<FScores> ScoresQ;
    TArray<FCurrent> CurrentQ;
    TArray<FSubmit> SubmitQ;
};

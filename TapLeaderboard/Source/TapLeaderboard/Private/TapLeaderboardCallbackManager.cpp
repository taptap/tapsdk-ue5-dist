#include "TapLeaderboardCallbackManager.h"

FTapLeaderboardCallbackManager& FTapLeaderboardCallbackManager::Get()
{
    static FTapLeaderboardCallbackManager Instance;
    return Instance;
}

void FTapLeaderboardCallbackManager::Reset()
{
    Events.Reset();
    Share.Reset();
    FScopeLock ScopeLock(&Lock);
    ScoresQ.Reset();
    CurrentQ.Reset();
    SubmitQ.Reset();
}

void FTapLeaderboardCallbackManager::EnqueueScores(
    const FString& Operation,
    const FTapLeaderboardScoresCallback& OnSuccess,
    const FTapLeaderboardErrorCallback& OnError, TFunction<void()> Dispatch)
{
    bool bStart;
    {
        FScopeLock ScopeLock(&Lock);
        bStart = !ScoresQ.ContainsByPredicate([&Operation](const FScores& Item) { return Item.Operation == Operation; });
        ScoresQ.Add({Operation, OnSuccess, OnError, Dispatch});
    }
    if (bStart) Dispatch();
}

void FTapLeaderboardCallbackManager::EnqueueCurrent(
    const FTapLeaderboardCurrentScoreCallback& OnSuccess,
    const FTapLeaderboardErrorCallback& OnError, TFunction<void()> Dispatch)
{
    bool bStart;
    {
        FScopeLock ScopeLock(&Lock);
        bStart = CurrentQ.IsEmpty();
        CurrentQ.Add({OnSuccess, OnError, Dispatch});
    }
    if (bStart) Dispatch();
}

void FTapLeaderboardCallbackManager::EnqueueSubmit(
    const FTapLeaderboardSubmitCallback& OnSuccess,
    const FTapLeaderboardErrorCallback& OnError, TFunction<void()> Dispatch)
{
    bool bStart;
    {
        FScopeLock ScopeLock(&Lock);
        bStart = SubmitQ.IsEmpty();
        SubmitQ.Add({OnSuccess, OnError, Dispatch});
    }
    if (bStart) Dispatch();
}

void FTapLeaderboardCallbackManager::Scores(const FString& Operation, const FTapLeaderboardScoresResult& Result)
{
    FScores Callback;
    TFunction<void()> Next;
    {
        FScopeLock ScopeLock(&Lock);
        const int32 Index = ScoresQ.IndexOfByPredicate([&Operation](const FScores& Item)
        {
            return Item.Operation == Operation;
        });
        if (Index == INDEX_NONE)
        {
            return;
        }
        Callback = ScoresQ[Index];
        ScoresQ.RemoveAt(Index);
        const FScores* Pending = ScoresQ.FindByPredicate([&Operation](const FScores& Item) { return Item.Operation == Operation; });
        if (Pending) Next = Pending->Dispatch;
    }
    // Dispatch the reserved next request before user code can enqueue or reset.
    // Native calls and user delegates must never execute while holding Lock.
    if (Next) Next();
    Callback.S.ExecuteIfBound(Result);
}

void FTapLeaderboardCallbackManager::Current(const FTapUserScoreResult& Result)
{
    FCurrent Callback;
    TFunction<void()> Next;
    {
        FScopeLock ScopeLock(&Lock);
        if (CurrentQ.IsEmpty()) return;
        Callback = CurrentQ[0];
        CurrentQ.RemoveAt(0);
        if (!CurrentQ.IsEmpty()) Next = CurrentQ[0].Dispatch;
    }
    if (Next) Next();
    Callback.S.ExecuteIfBound(Result);
}

void FTapLeaderboardCallbackManager::Submitted(const FTapSubmitScoresResult& Result)
{
    FSubmit Callback;
    TFunction<void()> Next;
    {
        FScopeLock ScopeLock(&Lock);
        if (SubmitQ.IsEmpty()) return;
        Callback = SubmitQ[0];
        SubmitQ.RemoveAt(0);
        if (!SubmitQ.IsEmpty()) Next = SubmitQ[0].Dispatch;
    }
    if (Next) Next();
    Callback.S.ExecuteIfBound(Result);
}

void FTapLeaderboardCallbackManager::Error(const FString& Operation, int32 Code, const FString& Message)
{
    FTapLeaderboardErrorCallback OnError;
    TFunction<void()> Next;
    {
        FScopeLock ScopeLock(&Lock);
        if (Operation == TEXT("loadCurrentPlayerScore") && !CurrentQ.IsEmpty())
        {
            OnError = CurrentQ[0].E;
            CurrentQ.RemoveAt(0);
            if (!CurrentQ.IsEmpty()) Next = CurrentQ[0].Dispatch;
        }
        else if (Operation == TEXT("submitScores") && !SubmitQ.IsEmpty())
        {
            OnError = SubmitQ[0].E;
            SubmitQ.RemoveAt(0);
            if (!SubmitQ.IsEmpty()) Next = SubmitQ[0].Dispatch;
        }
        else
        {
            const int32 Index = ScoresQ.IndexOfByPredicate([&Operation](const FScores& Item)
            {
                return Item.Operation == Operation;
            });
            if (Index != INDEX_NONE)
            {
                OnError = ScoresQ[Index].E;
                ScoresQ.RemoveAt(Index);
                const FScores* Pending = ScoresQ.FindByPredicate([&Operation](const FScores& Item) { return Item.Operation == Operation; });
                if (Pending) Next = Pending->Dispatch;
            }
        }
    }
    if (Next) Next();
    OnError.ExecuteIfBound(Operation, Code, Message);
}

void FTapLeaderboardCallbackManager::Event(int32 Code, const FString& Message)
{
    Events.Dispatch([Code, Message](UObject* Object)
    {
        ITapLeaderboardCallback::Execute_OnLeaderboardResult(Object, Code, Message);
    });
}

void FTapLeaderboardCallbackManager::SetShareCallback(UObject* Callback)
{
    // Replace the receiver and invalidate events queued for the previous registration.
    Share.Reset();
    Share.Register(Callback, UTapLeaderboardShareCallback::StaticClass());
}

void FTapLeaderboardCallbackManager::ShareResult(bool bSuccess, const FString& Data)
{
    Share.Dispatch([bSuccess, Data](UObject* Object)
    {
        if (bSuccess)
        {
            ITapLeaderboardShareCallback::Execute_OnShareSuccess(Object, Data);
        }
        else
        {
            ITapLeaderboardShareCallback::Execute_OnShareFailed(Object, Data);
        }
    });
}

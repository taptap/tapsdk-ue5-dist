#pragma once

#include "CoreMinimal.h"
#include "TapLeaderboard.h"
#include "TapLeaderboardCallbackManagerTestTypes.generated.h"

UCLASS()
class UTapLeaderboardCallbackManagerTestReceiver : public UObject, public ITapLeaderboardShareCallback
{
    GENERATED_BODY()
public:
    virtual void OnShareSuccess_Implementation(const FString& LocalPath) override
    {
        ++ShareSuccessCount;
        LastShareData = LocalPath;
    }
    virtual void OnShareFailed_Implementation(const FString& ErrorMessage) override
    {
        ++ShareFailureCount;
        LastShareData = ErrorMessage;
    }
    int32 ShareSuccessCount = 0;
    int32 ShareFailureCount = 0;
    FString LastShareData;

    void OnScores(const FTapLeaderboardScoresResult& Result)
    {
        ++SuccessCount;
        LastLeaderboardId = Result.Leaderboard.Id;
    }
    void OnCurrent(const FTapUserScoreResult& Result)
    {
        ++SuccessCount;
        LastLeaderboardId = Result.Leaderboard.Id;
    }
    void OnSubmit(const FTapSubmitScoresResult& Result)
    {
        ++SuccessCount;
        LastSubmitCount = Result.Items.Num();
    }
    void OnError(const FString& Operation, int32 Code, const FString& Message)
    {
        ++ErrorCount;
        LastOperation = Operation;
        LastErrorCode = Code;
    }
    int32 SuccessCount = 0;
    int32 ErrorCount = 0;
    int32 LastErrorCode = 0;
    int32 LastSubmitCount = 0;
    FString LastLeaderboardId;
    FString LastOperation;
};

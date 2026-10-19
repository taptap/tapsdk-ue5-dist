// Copyright TapTap. All Rights Reserved.
#include "TapLeaderboardResultParser.h"
#include "TapLeaderboardModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ─── 内部工具 ────────────────────────────────────────────────────────────────

namespace
{
    TSharedPtr<FJsonObject> ParseJson(const FString& Str)
    {
        TSharedPtr<FJsonObject> Obj;
        TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Str);
        FJsonSerializer::Deserialize(R, Obj);
        return Obj;
    }

    FString SafeStr(const TSharedPtr<FJsonObject>& O, const FString& Key)
    {
        FString V;
        if (O.IsValid()) O->TryGetStringField(Key, V);
        return V;
    }

    int32 SafeInt(const TSharedPtr<FJsonObject>& O, const FString& Key)
    {
        int32 V = 0;
        if (O.IsValid()) O->TryGetNumberField(Key, V);
        return V;
    }

    int64 SafeInt64(const TSharedPtr<FJsonObject>& O, const FString& Key)
    {
        int64 V = 0;
        if (O.IsValid())
        {
            double D = 0;
            if (O->TryGetNumberField(Key, D)) V = (int64)D;
        }
        return V;
    }

    FTapLeaderboardUser ParseUser(const TSharedPtr<FJsonObject>& O)
    {
        FTapLeaderboardUser U;
        if (!O.IsValid()) return U;
        U.OpenId  = SafeStr(O, TEXT("openid"));
        U.UnionId = SafeStr(O, TEXT("unionid"));
        U.Name    = SafeStr(O, TEXT("name"));
        if (!O->TryGetStringField(TEXT("avatar"), U.AvatarUrl))
        {
            // 兼容旧版桥接格式: {"avatar":{"url":"..."}}
            const TSharedPtr<FJsonObject>* AvatarObj = nullptr;
            if (O->TryGetObjectField(TEXT("avatar"), AvatarObj) && AvatarObj)
                U.AvatarUrl = SafeStr(*AvatarObj, TEXT("url"));
        }
        return U;
    }

    FTapLeaderboardScore ParseScore(const TSharedPtr<FJsonObject>& O)
    {
        FTapLeaderboardScore S;
        if (!O.IsValid()) return S;
        S.Rank               = SafeInt64(O, TEXT("rank"));
        S.RankDisplay        = SafeStr(O,   TEXT("rank_display"));
        S.Score              = SafeInt64(O, TEXT("score"));
        S.ScoreDisplay       = SafeStr(O,   TEXT("score_display"));
        S.ScoreSubmittedTime = SafeInt64(O, TEXT("score_submitted_time"));
        const TSharedPtr<FJsonObject>* UserObj = nullptr;
        if (O->TryGetObjectField(TEXT("user"), UserObj) && UserObj)
            S.User = ParseUser(*UserObj);
        return S;
    }

    FTapLeaderboardPeriod ParsePeriod(const TSharedPtr<FJsonObject>& O)
    {
        FTapLeaderboardPeriod P;
        if (!O.IsValid()) return P;
        P.Display     = SafeStr(O, TEXT("display"));
        P.PeriodToken = SafeStr(O, TEXT("period_token"));
        return P;
    }

    FTapLeaderboardInfo ParseInfo(const TSharedPtr<FJsonObject>& O)
    {
        FTapLeaderboardInfo I;
        if (!O.IsValid()) return I;
        I.Id   = SafeStr(O, TEXT("id"));
        I.Name = SafeStr(O, TEXT("name"));

        const TArray<TSharedPtr<FJsonValue>>* PeriodsArr = nullptr;
        if (O->TryGetArrayField(TEXT("available_periods"), PeriodsArr) && PeriodsArr)
        {
            for (const TSharedPtr<FJsonValue>& V : *PeriodsArr)
            {
                if (V.IsValid() && V->Type == EJson::Object)
                    I.AvailablePeriods.Add(ParsePeriod(V->AsObject()));
            }
        }

        const TSharedPtr<FJsonObject>* PeriodObj = nullptr;
        if (O->TryGetObjectField(TEXT("period"), PeriodObj) && PeriodObj)
            I.Period = ParsePeriod(*PeriodObj);

        const TSharedPtr<FJsonObject>* ScoreObj = nullptr;
        if (O->TryGetObjectField(TEXT("score"), ScoreObj) && ScoreObj)
            I.Score = ParseScore(*ScoreObj);

        return I;
    }
}

// ─── Public ──────────────────────────────────────────────────────────────────

TapLeaderboardResultParser::FParsed TapLeaderboardResultParser::Parse(const FString& Json)
{
    FParsed P;
    TSharedPtr<FJsonObject> Root = ParseJson(Json);
    if (!Root.IsValid()) { UE_LOG(LogTapLeaderboard, Warning, TEXT("[Parser] invalid JSON: %s"), *Json); return P; }

    P.bParsed = true;
    P.Op      = SafeStr(Root, TEXT("op"));
    FString State = SafeStr(Root, TEXT("state"));

    if (State == TEXT("success"))
    {
        P.bIsSuccess = true;
        P.Data       = SafeStr(Root, TEXT("data"));
    }
    else if (State == TEXT("failure"))
    {
        P.bIsSuccess = false;
        P.ErrCode    = SafeInt(Root, TEXT("errCode"));
        P.ErrMsg     = SafeStr(Root, TEXT("errMessage"));
    }
    else if (State == TEXT("event"))
    {
        P.bIsEvent     = true;
        P.EventCode    = SafeInt(Root, TEXT("code"));
        P.EventMessage = SafeStr(Root, TEXT("message"));
    }
    return P;
}

FTapLeaderboardScoresResult TapLeaderboardResultParser::ParseScoresResult(const FString& DataJson)
{
    FTapLeaderboardScoresResult R;
    TSharedPtr<FJsonObject> Root = ParseJson(DataJson);
    if (!Root.IsValid()) return R;

    const TSharedPtr<FJsonObject>* LbObj = nullptr;
    if (Root->TryGetObjectField(TEXT("leaderboard"), LbObj) && LbObj)
        R.Leaderboard = ParseInfo(*LbObj);

    R.NextPage = SafeStr(Root, TEXT("next_page"));

    const TArray<TSharedPtr<FJsonValue>>* ScoresArr = nullptr;
    if (Root->TryGetArrayField(TEXT("scores"), ScoresArr) && ScoresArr)
    {
        for (const TSharedPtr<FJsonValue>& V : *ScoresArr)
        {
            if (V.IsValid() && V->Type == EJson::Object)
                R.Scores.Add(ParseScore(V->AsObject()));
        }
    }
    return R;
}

FTapUserScoreResult TapLeaderboardResultParser::ParseUserScoreResult(const FString& DataJson)
{
    FTapUserScoreResult R;
    TSharedPtr<FJsonObject> Root = ParseJson(DataJson);
    if (!Root.IsValid()) return R;

    const TSharedPtr<FJsonObject>* LbObj = nullptr;
    if (Root->TryGetObjectField(TEXT("leaderboard"), LbObj) && LbObj)
        R.Leaderboard = ParseInfo(*LbObj);

    const TSharedPtr<FJsonObject>* ScoreObj = nullptr;
    if (Root->TryGetObjectField(TEXT("current_user_score"), ScoreObj) && ScoreObj)
        R.CurrentUserScore = ParseScore(*ScoreObj);

    return R;
}

FTapSubmitScoresResult TapLeaderboardResultParser::ParseSubmitResult(const FString& DataJson)
{
    FTapSubmitScoresResult R;
    TSharedPtr<FJsonObject> Root = ParseJson(DataJson);
    if (!Root.IsValid()) return R;

    const TArray<TSharedPtr<FJsonValue>>* ResultsArr = nullptr;
    if (!Root->TryGetArrayField(TEXT("results"), ResultsArr) || !ResultsArr) return R;

    for (const TSharedPtr<FJsonValue>& V : *ResultsArr)
    {
        if (!V.IsValid() || V->Type != EJson::Object) continue;
        TSharedPtr<FJsonObject> O = V->AsObject();

        FTapSubmitScoreResultItem Item;
        Item.LeaderboardId = SafeStr(O, TEXT("leaderboard_id"));
        Item.OpenId        = SafeStr(O, TEXT("openid"));
        Item.UnionId       = SafeStr(O, TEXT("unionid"));
        Item.PeriodToken   = SafeStr(O, TEXT("period_token"));

        const TSharedPtr<FJsonObject>* ScoreResultObj = nullptr;
        if (O->TryGetObjectField(TEXT("score_result"), ScoreResultObj) && ScoreResultObj)
        {
            Item.bNewBest     = false;
            (*ScoreResultObj)->TryGetBoolField(TEXT("new_best"), Item.bNewBest);
            Item.RawScore     = SafeInt64(*ScoreResultObj, TEXT("raw_score"));
            Item.ScoreDisplay = SafeStr(*ScoreResultObj, TEXT("score_display"));
        }
        R.Items.Add(Item);
    }
    return R;
}


// Copyright TapTap. All Rights Reserved.
#include "TapAchievementResultParser.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    ETapAchievementType ParseType(const FString& S)
    {
        return S.Equals(TEXT("PLATINUM"), ESearchCase::IgnoreCase)
            ? ETapAchievementType::Platinum
            : ETapAchievementType::Normal;
    }

    void ParseResultObject(const TSharedPtr<FJsonObject>& Obj, FTapAchievementResult& Out)
    {
        if (!Obj.IsValid()) { return; }
        Obj->TryGetStringField(TEXT("achievementId"),   Out.AchievementId);
        Obj->TryGetStringField(TEXT("achievementName"), Out.AchievementName);

        FString TypeStr;
        if (Obj->TryGetStringField(TEXT("achievementType"), TypeStr))
        {
            Out.AchievementType = ParseType(TypeStr);
        }
        int64 Steps = 0;
        if (Obj->TryGetNumberField(TEXT("currentSteps"), Steps))
        {
            Out.CurrentSteps = Steps;
        }
    }
}

namespace TapAchievementResultParser
{
    bool ParseAchievementBean(const TSharedPtr<FJsonObject>& Bean,
                              ETapAchievementType Type, FTapAchievementResult& OutResult)
    {
        if (!Bean.IsValid()) return false;
        OutResult.AchievementId   = Bean->GetStringField(TEXT("id"));
        OutResult.AchievementName = Bean->GetStringField(TEXT("name"));
        OutResult.AchievementType = Type;
        OutResult.CurrentSteps = 0;
        Bean->TryGetNumberField(TEXT("current_steps"), OutResult.CurrentSteps);
        return true;
    }

    FParsed Parse(const FString& Json)
    {
        FParsed P;

        TSharedPtr<FJsonObject> Root;
        auto Reader = TJsonReaderFactory<>::Create(Json);
        // 两层 JSON 均保留数字原文，int64 不经 double（2^53 以上会丢精度）。
        if (!FJsonSerializer::Deserialize(Reader, Root, FJsonSerializer::EFlags::StoreNumbersAsStrings) || !Root.IsValid())
        {
            return P;  // bParsed = false
        }
        P.bParsed = true;

        FString State;
        Root->TryGetStringField(TEXT("state"), State);
        P.bIsSuccess = State.Equals(TEXT("success"), ESearchCase::IgnoreCase);

        if (P.bIsSuccess)
        {
            int32 Code = 0;
            if (Root->TryGetNumberField(TEXT("code"), Code)) { P.Code = Code; }

            // result 可能是内嵌 JSON 字符串(Android/iOS 序列化习惯)或直接对象，两种都兼容。
            FString ResultStr;
            const TSharedPtr<FJsonObject>* ResultObjPtr = nullptr;
            if (Root->TryGetStringField(TEXT("result"), ResultStr) && !ResultStr.IsEmpty())
            {
                TSharedPtr<FJsonObject> ResultObj;
                auto ResultReader = TJsonReaderFactory<>::Create(ResultStr);
                if (FJsonSerializer::Deserialize(ResultReader, ResultObj, FJsonSerializer::EFlags::StoreNumbersAsStrings) && ResultObj.IsValid())
                {
                    ParseResultObject(ResultObj, P.Result);
                }
            }
            else if (Root->TryGetObjectField(TEXT("result"), ResultObjPtr) && ResultObjPtr)
            {
                ParseResultObject(*ResultObjPtr, P.Result);
            }
        }
        else
        {
            Root->TryGetStringField(TEXT("achievementId"), P.AchievementId);
            int32 ErrCode = 0;
            if (Root->TryGetNumberField(TEXT("errorCode"), ErrCode)) { P.ErrorCode = ErrCode; }
            Root->TryGetStringField(TEXT("errorMsg"), P.ErrorMsg);
        }
        return P;
    }
}

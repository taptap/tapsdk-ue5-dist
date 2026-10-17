#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace TapRelationLiteInviteParser
{
    inline bool Parse(const FString& Json, FString& OpenId, FString& UnionId, FString& TeamId)
    {
        OpenId.Reset();
        UnionId.Reset();
        TeamId.Reset();
        TSharedPtr<FJsonObject> Result;
        const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
        if (!FJsonSerializer::Deserialize(Reader, Result) || !Result.IsValid()) return false;
        // These names are the Android/iOS native invite callback wire format.
        Result->TryGetStringField(TEXT("open_id"), OpenId);
        Result->TryGetStringField(TEXT("union_id"), UnionId);
        Result->TryGetStringField(TEXT("team_id"), TeamId);
        return true;
    }
}

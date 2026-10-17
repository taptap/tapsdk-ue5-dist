// Copyright TapTap. All Rights Reserved.
#include "TapRelationLiteCallbackUtils.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void TapRelationLiteCallbackUtils::Dispatch(TFunction<void()> Callback, TFunction<bool()> IsCurrent)
{
    AsyncTask(ENamedThreads::GameThread,
        [Callback = MoveTemp(Callback), IsCurrent = MoveTemp(IsCurrent)]
        {
            if ((!IsCurrent || IsCurrent()) && Callback) Callback();
        });
}

void TapRelationLiteCallbackUtils::CompleteSync(const FString& Json, TFunction<void()> OnSuccess,
    TFunction<void(const FString&)> OnFailure, TFunction<bool()> IsCurrent)
{
    TSharedPtr<FJsonObject> Root;
    const auto Reader = TJsonReaderFactory<>::Create(Json);
    double Status = -1;
    FString Error = TEXT("Invalid RelationLite sync response");
    bool bSuccess = false;
    if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
    {
        bSuccess = Root->TryGetNumberField(TEXT("success"), Status) && Status == 0 &&
            !Root->HasField(TEXT("error_message"));
        FString NativeError;
        if (Root->TryGetStringField(TEXT("error_message"), NativeError) && !NativeError.IsEmpty()) Error = NativeError;
    }
    Dispatch([bSuccess, Error, OnSuccess = MoveTemp(OnSuccess), OnFailure = MoveTemp(OnFailure)]
    {
        if (bSuccess) { if (OnSuccess) OnSuccess(); }
        else { if (OnFailure) OnFailure(Error); }
    }, MoveTemp(IsCurrent));
}

bool TapRelationLiteCallbackUtils::ParseResultCode(const FString& Json, int32& Code)
{
    TSharedPtr<FJsonObject> Root;
    const auto Reader = TJsonReaderFactory<>::Create(Json);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return false;
    // Swift emits a number. Accept the legacy string representation as well.
    FString Value;
    return Root->TryGetStringField(TEXT("relation_lite_result_code"), Value) && LexTryParseString(Code, *Value);
}

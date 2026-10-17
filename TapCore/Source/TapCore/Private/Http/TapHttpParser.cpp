// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttpParser.h"
#include "Internal/Http/TapHttpTypes.h"     // FTapHttpServerResponse, FTapHttpErrorData
#include "Internal/Http/TapHttpException.h" // TapHttpExceptionFactory
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    class FTapHttpEnvelopeParser : public ITapHttpParser
    {
    public:
        explicit FTapHttpEnvelopeParser(bool bStoreNumbersAsStrings = false)
            : JsonFlags(bStoreNumbersAsStrings ? FJsonSerializer::EFlags::StoreNumbersAsStrings
                                               : FJsonSerializer::EFlags::None) {}

        virtual FTapHttpJsonResult Parse(
            int32 StatusCode, const FString& Content,
            bool bHttpOk, const FString& ErrorMessage) override
        {
            // 网络错误
            if (!bHttpOk || StatusCode == 0)
            {
                return FTapHttpJsonResult::NetworkError(
                    ErrorMessage.IsEmpty() ? TEXT("Network error") : ErrorMessage);
            }

            // 解析 JSON
            TSharedPtr<FJsonObject> JsonObject;
            if (!Content.IsEmpty())
            {
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
                if (!FJsonSerializer::Deserialize(Reader, JsonObject, JsonFlags) || !JsonObject.IsValid())
                {
                    return FTapHttpJsonResult::InvalidResponse(StatusCode,
                        FString::Printf(TEXT("Failed to parse JSON: %s"), *Content));
                }
            }

            // 检查 success
            bool bSuccess = JsonObject.IsValid() && JsonObject->GetBoolField(TEXT("success"));

            // 取出 now (envelope 字段), 不在 parser 里 FixTime —— parser 是纯解析,
            // 实际全局时钟校准由 TapHttpRequest 的响应回调统一负责 (cpp:291 / cpp:471).
            // 若在这里 FixTime, 纯单元测试喂入 "now":123 等假数据会污染全局单例,
            // 导致后续真实请求的 X-Tap-Ts 用错误时钟, 触发 invalid_time。
            int64 ServerTime = 0;
            if (JsonObject.IsValid())
            {
                JsonObject->TryGetNumberField(TEXT("now"), ServerTime);
            }

            // HTTP 非 2xx
            if (StatusCode < 200 || StatusCode >= 300)
            {
                FTapHttpServerResponse Response;
                Response.bSuccess = false;
                Response.Now = ServerTime;
                Response.Data = JsonObject;

                FTapHttpErrorData ErrorData;
                if (JsonObject.IsValid())
                {
                    if (JsonObject->HasField(TEXT("data")))
                    {
                        TSharedPtr<FJsonObject> DataObj = JsonObject->GetObjectField(TEXT("data"));
                        if (DataObj.IsValid())
                        {
                            ErrorData.Code = DataObj->GetIntegerField(TEXT("code"));
                            ErrorData.Error = DataObj->GetStringField(TEXT("error"));
                            ErrorData.Msg = DataObj->GetStringField(TEXT("msg"));
                            ErrorData.ErrorDescription = DataObj->GetStringField(TEXT("error_description"));
                        }
                    }
                    if (ErrorData.Error.IsEmpty())
                    {
                        ErrorData.Code = JsonObject->GetIntegerField(TEXT("code"));
                        ErrorData.Error = JsonObject->GetStringField(TEXT("error"));
                        ErrorData.Msg = JsonObject->GetStringField(TEXT("msg"));
                        if (ErrorData.Msg.IsEmpty()) ErrorData.Msg = JsonObject->GetStringField(TEXT("message"));
                        ErrorData.ErrorDescription = JsonObject->GetStringField(TEXT("error_description"));
                    }
                }
                if (ErrorData.Msg.IsEmpty())
                {
                    ErrorData.Msg = FString::Printf(TEXT("HTTP %d"), StatusCode);
                }
                Response.ErrorData = ErrorData;
                return FTapHttpJsonResult::ServerError(StatusCode, Response, ErrorData);
            }

            // success=true: 提取 data 子对象 (没有就用整个 envelope)
            if (bSuccess)
            {
                TSharedPtr<FJsonObject> Data;
                if (JsonObject.IsValid() && JsonObject->HasField(TEXT("data")))
                {
                    Data = JsonObject->GetObjectField(TEXT("data"));
                }
                else
                {
                    Data = JsonObject;
                }
                return FTapHttpJsonResult::Success(Data);
            }

            // success=false 但 HTTP 2xx
            FTapHttpServerResponse Response;
            Response.bSuccess = false;
            Response.Now = ServerTime;
            Response.Data = JsonObject;

            FTapHttpErrorData ErrorData;
            if (JsonObject.IsValid() && JsonObject->HasField(TEXT("data")))
            {
                TSharedPtr<FJsonObject> DataObj = JsonObject->GetObjectField(TEXT("data"));
                if (DataObj.IsValid())
                {
                    ErrorData.Code = DataObj->GetIntegerField(TEXT("code"));
                    ErrorData.Error = DataObj->GetStringField(TEXT("error"));
                    ErrorData.Msg = DataObj->GetStringField(TEXT("msg"));
                    ErrorData.ErrorDescription = DataObj->GetStringField(TEXT("error_description"));
                }
            }
            if (ErrorData.Msg.IsEmpty())
            {
                ErrorData.Msg = TEXT("Request failed");
            }
            Response.ErrorData = ErrorData;
            return FTapHttpJsonResult::ServerError(StatusCode, Response, ErrorData);
        }
    private:
        FJsonSerializer::EFlags JsonFlags;
    };
}

TSharedRef<ITapHttpParser> FTapHttpParser::CreateEnvelopeParser()
{
    return MakeShared<FTapHttpEnvelopeParser>();
}

TSharedRef<ITapHttpParser> FTapHttpParser::CreateEnvelopeParser(bool bStoreNumbersAsStrings)
{
    return MakeShared<FTapHttpEnvelopeParser>(bStoreNumbersAsStrings);
}

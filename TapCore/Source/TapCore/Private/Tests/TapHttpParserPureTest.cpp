// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internal/Http/TapHttpParser.h"
#include "Internal/Http/TapHttpException.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpParserEnvelopeSuccessTest,
    "TapBasic.Http.Pure.Parser.EnvelopeSuccess",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpParserEnvelopeSuccessTest::RunTest(const FString&)
{
    auto P = FTapHttpParser::CreateEnvelopeParser();
    auto R = P->Parse(200, TEXT("{\"success\":true,\"now\":123,\"data\":{\"k\":1}}"),
                     true, TEXT(""));
    TestTrue(TEXT("IsSuccess"), R.IsSuccess());
    TestTrue(TEXT("Data 不为 null"), R.GetData().IsValid());
    int32 V; R.GetData()->TryGetNumberField(TEXT("k"), V);
    TestEqual(TEXT("Data.k=1"), V, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpParserEnvelopeBusinessErrorTest,
    "TapBasic.Http.Pure.Parser.EnvelopeBusinessError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpParserEnvelopeBusinessErrorTest::RunTest(const FString&)
{
    auto P = FTapHttpParser::CreateEnvelopeParser();
    auto R = P->Parse(400,
        TEXT("{\"success\":false,\"data\":{\"error\":\"invalid_time\",\"msg\":\"ts\"}}"),
        true, TEXT(""));
    TestFalse(TEXT("IsSuccess=false"), R.IsSuccess());
    auto Exc = R.GetException();
    TestTrue(TEXT("Exception 非空"), Exc.IsValid());
    auto* Server = static_cast<FTapHttpServerException*>(Exc.Get());
    TestEqual(TEXT("Error=invalid_time"),
        Server->GetErrorData().Error, FString(TEXT("invalid_time")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpParserEnvelopeTopLevelFallbackTest,
    "TapBasic.Http.Pure.Parser.EnvelopeTopLevelFallback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpParserEnvelopeTopLevelFallbackTest::RunTest(const FString&)
{
    auto P = FTapHttpParser::CreateEnvelopeParser();
    auto R = P->Parse(400,
        TEXT("{\"success\":false,\"error\":\"top_err\"}"),
        true, TEXT(""));
    TestFalse(TEXT("IsSuccess=false"), R.IsSuccess());
    auto Exc = R.GetException();
    auto* Server = static_cast<FTapHttpServerException*>(Exc.Get());
    TestEqual(TEXT("data 缺失时从顶层取 error"),
        Server->GetErrorData().Error, FString(TEXT("top_err")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpParserEnvelopeInvalidJsonTest,
    "TapBasic.Http.Pure.Parser.EnvelopeInvalidJson",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpParserEnvelopeInvalidJsonTest::RunTest(const FString&)
{
    auto P = FTapHttpParser::CreateEnvelopeParser();
    auto R = P->Parse(200, TEXT("not json"), true, TEXT(""));
    TestFalse(TEXT("IsSuccess=false"), R.IsSuccess());
    auto Exc = R.GetException();
    TestEqual(TEXT("Exception 类型为 InvalidResponse"),
        Exc->GetErrorType(), ETapHttpErrorType::InvalidResponse);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpParserEnvelopeNetworkErrorTest,
    "TapBasic.Http.Pure.Parser.EnvelopeNetworkError",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpParserEnvelopeNetworkErrorTest::RunTest(const FString&)
{
    auto P = FTapHttpParser::CreateEnvelopeParser();
    auto R = P->Parse(0, TEXT(""), false, TEXT("conn refused"));
    TestFalse(TEXT("IsSuccess=false"), R.IsSuccess());
    auto Exc = R.GetException();
    TestEqual(TEXT("Exception 类型为 NetworkError"),
        Exc->GetErrorType(), ETapHttpErrorType::NetworkError);
    TestEqual(TEXT("ErrorMessage 透传"),
        Exc->GetMessage(), FString(TEXT("conn refused")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpParserEnvelopeEmptyBodyTest,
    "TapBasic.Http.Pure.Parser.EnvelopeEmptyBody",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpParserEnvelopeEmptyBodyTest::RunTest(const FString&)
{
    auto P = FTapHttpParser::CreateEnvelopeParser();
    auto R = P->Parse(204, TEXT(""), true, TEXT(""));
    // 空 body + 2xx: success=false (因为 JsonObject 为 null), 进入 success=false 分支
    // 但 StatusCode 2xx 不进 ServerError 分支, 而是 success=false 分支
    TestFalse(TEXT("IsSuccess=false"), R.IsSuccess());
    return true;
}

#endif

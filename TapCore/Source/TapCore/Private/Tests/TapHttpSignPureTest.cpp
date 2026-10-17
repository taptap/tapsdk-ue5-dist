// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internal/Http/TapHttpSign.h"
#include "Internal/Http/TapHttpCommonParams.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpSignNoneReturnsEmptyTest,
    "TapBasic.Http.Pure.Sign.None.ReturnsEmpty",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpSignNoneReturnsEmptyTest::RunTest(const FString&)
{
    auto None = FTapHttpSign::CreateNoneSign();
    TestEqual(TEXT("None.GetFixedHeaders 为空"),
        None->GetFixedHeaders(TEXT("https://x/a"), TEXT("GET"),
            TEXT("M"), TEXT("1"), false).Num(), 0);
    TestEqual(TEXT("None.GetFixedQueryParams 为空"),
        None->GetFixedQueryParams().Num(), 0);

    TMap<FString,FString> H;
    FTapHttpSignData D{TEXT("https://x/a"), TEXT("GET"), false, TEXT(""), H};
    None->Sign(D);
    TestEqual(TEXT("None.Sign 不动 Headers"), H.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpSignDefaultFixedHeadersGetTest,
    "TapBasic.Http.Pure.Sign.Default.FixedHeaders.GET",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpSignDefaultFixedHeadersGetTest::RunTest(const FString&)
{
    FTapHttpCommonParams::Initialize(TEXT("cid"), TEXT("ctoken"));
    auto S = FTapHttpSign::CreateDefaultSign();
    auto H = S->GetFixedHeaders(TEXT("https://x/a"), TEXT("GET"),
        TEXT("TapCompliance"), TEXT("1.2.3"), false);
    TestTrue(TEXT("含 X-Tap-PN"), H.Contains(TEXT("X-Tap-PN")));
    TestEqual(TEXT("X-Tap-PN=TapSDK"), H[TEXT("X-Tap-PN")], FString(TEXT("TapSDK")));
    TestEqual(TEXT("X-Tap-SDK-Module"), H[TEXT("X-Tap-SDK-Module")],
        FString(TEXT("TapCompliance")));
    TestEqual(TEXT("X-Tap-SDK-Module-Version"), H[TEXT("X-Tap-SDK-Module-Version")],
        FString(TEXT("1.2.3")));
    TestTrue(TEXT("含 X-Tap-Ts"), H.Contains(TEXT("X-Tap-Ts")));
    TestTrue(TEXT("含 X-Tap-Nonce"), H.Contains(TEXT("X-Tap-Nonce")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpSignDefaultFixedHeadersPostTest,
    "TapBasic.Http.Pure.Sign.Default.FixedHeaders.POST",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpSignDefaultFixedHeadersPostTest::RunTest(const FString&)
{
    FTapHttpCommonParams::Initialize(TEXT("cid"), TEXT("ctoken"));
    auto S = FTapHttpSign::CreateDefaultSign();
    auto H = S->GetFixedHeaders(TEXT("https://x/a"), TEXT("POST"),
        TEXT("M"), TEXT("1"), false);
    TestTrue(TEXT("POST 也返回完整公共头"), H.Contains(TEXT("X-Tap-PN")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpSignDefaultFixedHeadersOtherMethodTest,
    "TapBasic.Http.Pure.Sign.Default.FixedHeaders.OtherMethod",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpSignDefaultFixedHeadersOtherMethodTest::RunTest(const FString&)
{
    auto S = FTapHttpSign::CreateDefaultSign();
    auto H = S->GetFixedHeaders(TEXT("https://x/a"), TEXT("PUT"),
        TEXT("M"), TEXT("1"), false);
    TestEqual(TEXT("PUT 返回空 (对齐 Unity)"), H.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTapHttpSignDefaultSignatureDeterministicTest,
    "TapBasic.Http.Pure.Sign.Default.SignatureDeterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapHttpSignDefaultSignatureDeterministicTest::RunTest(const FString&)
{
    FTapHttpCommonParams::Initialize(TEXT("cid"), TEXT("ctoken"));
    auto S = FTapHttpSign::CreateDefaultSign();

    TMap<FString,FString> H1;
    H1.Add(TEXT("X-Tap-PN"),    TEXT("TapSDK"));
    H1.Add(TEXT("X-Tap-Ts"),    TEXT("1700000000"));
    H1.Add(TEXT("X-Tap-Nonce"), TEXT("abc1234567"));
    FTapHttpSignData D1{TEXT("https://x/a?b=1"), TEXT("GET"), false, TEXT(""), H1};
    S->Sign(D1);
    const FString Sign1 = H1.FindRef(TEXT("X-Tap-Sign"));
    TestTrue(TEXT("X-Tap-Sign 已写入"), !Sign1.IsEmpty());

    TMap<FString,FString> H2 = H1;
    H2.Remove(TEXT("X-Tap-Sign"));
    FTapHttpSignData D2{TEXT("https://x/a?b=1"), TEXT("GET"), false, TEXT(""), H2};
    S->Sign(D2);
    TestEqual(TEXT("同 input → 同 sign"), H2[TEXT("X-Tap-Sign")], Sign1);
    return true;
}

#endif

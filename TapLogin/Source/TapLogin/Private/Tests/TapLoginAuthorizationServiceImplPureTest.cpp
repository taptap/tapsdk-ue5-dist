// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapLoginAuthorizationServiceImpl.h"
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"
#include "Internal/Http/TapHttpTime.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLoginVersionTest,
	"TapSDK.Release.Version.Login",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLoginVersionTest::RunTest(const FString&)
{
	const bool bWasInitialized = FTapLoginInternal::IsInitialized();
	TestEqual(TEXT("Login version follows the packaged plugin descriptor"),
		FTapLoginInternal::GetVersion(), FString(TapLogin_VERSION));
	TestEqual(TEXT("Reading the version does not initialize Login"),
		FTapLoginInternal::IsInitialized(), bWasInitialized);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLoginAuthImplNoTokenTest,
	"TapLogin.Auth.Pure.Impl.NoToken",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLoginAuthImplNoTokenTest::RunTest(const FString&)
{
	// 不注入 token, 预期返回空
	FTapLoginAuthorizationServiceImpl Impl;
	const FString Got = Impl.ObtainAuthorization(
		TEXT("https://x.com/p"), TEXT("GET"));
	TestTrue(TEXT("未登录 → 返回空"), Got.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLoginAuthImplMacFormatTest,
	"TapLogin.Auth.Pure.Impl.MacFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLoginAuthImplMacFormatTest::RunTest(const FString&)
{
	// 用 baseline commit 4d61b9466f 的 InjectTokenForTest 注入
	FTapLoginInternal::InjectTokenForTest(
		TEXT("test_kid"), TEXT("test_access_token"),
		TEXT("test_mac_key"), TEXT("hmac-sha-1"),
		TArray<FString>{TEXT("public_profile")});

	FTapLoginAuthorizationServiceImpl Impl;
	const FString Got = Impl.ObtainAuthorization(
		TEXT("https://x.com/p?q=1"), TEXT("GET"));
	TestTrue(TEXT("以 MAC 开头"), Got.StartsWith(TEXT("MAC ")));
	TestTrue(TEXT("含 id="),    Got.Contains(TEXT("id=")));
	TestTrue(TEXT("含 ts="),    Got.Contains(TEXT("ts=")));
	TestTrue(TEXT("含 nonce="), Got.Contains(TEXT("nonce=")));
	TestTrue(TEXT("含 mac="),   Got.Contains(TEXT("mac=")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLoginExplicitTokenMacFormatTest,
	"TapLogin.Auth.Pure.ExplicitToken.MacFormat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLoginExplicitTokenMacFormatTest::RunTest(const FString&)
{
	FTapAccessToken Token;
	Token.Kid = TEXT("explicit_kid");
	Token.TokenType = TEXT("mac");
	Token.MacKey = TEXT("explicit_mac_key");
	Token.MacAlgorithm = TEXT("hmac-sha-1");
	Token.Scopes.Add(TEXT("compliance_basic"));

	const FString Got = FTapLoginInternal::BuildAuthorizationHeaderForToken(
		Token,
		TEXT("GET"),
		TEXT("/real-name/v1/anti-addiction-token-taptap?client_id=test&user_identifier=u"),
		TEXT("tapsdk.tapapis.cn"),
		TEXT("443"),
		123456789);

	TestTrue(TEXT("explicit token MAC starts with MAC"), Got.StartsWith(TEXT("MAC ")));
	TestTrue(TEXT("explicit token MAC contains kid"), Got.Contains(TEXT("id=\"explicit_kid\"")));
	TestTrue(TEXT("explicit token MAC contains fixed ts"), Got.Contains(TEXT("ts=\"123456789\"")));
	TestTrue(TEXT("explicit token MAC contains mac"), Got.Contains(TEXT("mac=")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLoginAuthImplUsesCalibratedClockTest,
	"TapLogin.Auth.Pure.Impl.UsesCalibratedClock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLoginAuthImplUsesCalibratedClockTest::RunTest(const FString&)
{
	FTapLoginInternal::InjectTokenForTest(
		TEXT("test_kid"), TEXT("test_access_token"),
		TEXT("test_mac_key"), TEXT("hmac-sha-1"),
		TArray<FString>{TEXT("public_profile")});

	// 调一次拿当前 ts
	FTapLoginAuthorizationServiceImpl Impl;
	const FString Got1 = Impl.ObtainAuthorization(TEXT("https://x.com/p"), TEXT("GET"));

	// 校准时钟到未来 1 小时
	int64 Future = FTapHttpTime::GetCurrentTime() + 3600;
	FTapHttpTime::FixTime(Future);

	// 再调一次, ts 应该反映校准
	const FString Got2 = Impl.ObtainAuthorization(TEXT("https://x.com/p"), TEXT("GET"));

	// 两次结果不同 (ts 不同)
	TestNotEqual(TEXT("校准前后两次 MAC 不同 (ts 已变)"), Got1, Got2);

	// 恢复时钟 (回到本地时间附近, 避免污染其它测试)
	FTapHttpTime::FixTime(Future - 3600);
	return true;
}

#endif

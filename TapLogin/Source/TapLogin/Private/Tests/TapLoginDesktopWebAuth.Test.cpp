// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "TapLoginDesktopWebAuth.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLoginDesktopWebAuthURLTest,
	"TapLogin.Desktop.WebAuth.AuthorizationURL",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLoginDesktopWebAuthURLTest::RunTest(const FString&)
{
	const FString URL = FTapLoginDesktopWebAuth::BuildAuthorizationURL(
		TEXT("https://accounts.example.com/authorize"),
		TEXT("client id"),
		TEXT("http://127.0.0.1:18787/authorize"),
		TEXT("expected-state"),
		TEXT("challenge"),
		TEXT("public_profile,basic_info"));

	TestTrue(TEXT("localhost redirect is percent-encoded"),
		URL.Contains(TEXT("redirect_uri=http%3A%2F%2F127.0.0.1%3A18787%2Fauthorize")));
	TestFalse(TEXT("flow is not appended"), URL.Contains(TEXT("flow=")));
	TestTrue(TEXT("client ID is encoded"), URL.Contains(TEXT("client_id=client%20id")));
	TestTrue(TEXT("scope is encoded"), URL.Contains(TEXT("scope=public_profile%2Cbasic_info")));

	const FString ComplianceURL = FTapLoginDesktopWebAuth::BuildAuthorizationURL(
		TEXT("https://accounts.example.com/authorize"),
		TEXT("client-id"),
		TEXT("open-taptap-client-id://authorize"),
		TEXT("state"),
		TEXT("challenge"),
		TEXT("basic_info,public_profile,compliance_basic"));
	TestTrue(TEXT("compliance scope is preserved"),
		ComplianceURL.Contains(TEXT("scope=basic_info%2Cpublic_profile%2Ccompliance_basic")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLoginDesktopWebAuthCallbackTest,
	"TapLogin.Desktop.WebAuth.CallbackValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLoginDesktopWebAuthCallbackTest::RunTest(const FString&)
{
	FString Code;
	TestTrue(TEXT("matching state accepts callback"),
		FTapLoginDesktopWebAuth::TryParseCallbackPath(
			TEXT("/authorize?code=a%2Bb&state=expected"), TEXT("expected"), Code));
	TestEqual(TEXT("code is percent-decoded"), Code, TEXT("a+b"));

	TestFalse(TEXT("mismatched state rejects callback"),
		FTapLoginDesktopWebAuth::TryParseCallbackPath(
			TEXT("/authorize?code=abc&state=other"), TEXT("expected"), Code));
	TestTrue(TEXT("rejected callback clears code"), Code.IsEmpty());

	TestFalse(TEXT("missing code rejects callback"),
		FTapLoginDesktopWebAuth::TryParseCallbackPath(
			TEXT("/authorize?state=expected"), TEXT("expected"), Code));
	TestFalse(TEXT("different path rejects callback"),
		FTapLoginDesktopWebAuth::TryParseCallbackPath(
			TEXT("/other?code=abc&state=expected"), TEXT("expected"), Code));

	FString Error;
	TestTrue(TEXT("access denied callback is accepted as a terminal error"),
		FTapLoginDesktopWebAuth::TryParseCallbackPath(
			TEXT("/authorize?error=access_denied&state=expected"), TEXT("expected"), Code, Error));
	TestTrue(TEXT("access denied error is returned"), Error == TEXT("access_denied"));
	TestTrue(TEXT("error callback does not expose a code"), Code.IsEmpty());

	TestFalse(TEXT("error callback with mismatched state is rejected"),
		FTapLoginDesktopWebAuth::TryParseCallbackPath(
			TEXT("/authorize?error=access_denied&state=other"), TEXT("expected"), Code, Error));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && desktop

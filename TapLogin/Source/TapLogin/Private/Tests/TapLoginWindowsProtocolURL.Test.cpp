// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "TapDeepLinkHandler.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLoginWindowsProtocolURLTest,
	"TapLogin.Desktop.Windows.ProtocolURLValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapLoginWindowsProtocolURLTest::RunTest(const FString&)
{
	FString ClientID;
	TestTrue(TEXT("authorize callback is accepted"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-client_123://authorize?code=value&state=value"), ClientID));
	TestEqual(TEXT("ClientID is extracted"), ClientID, TEXT("client_123"));

	TestTrue(TEXT("authorize callback with trailing slash is accepted"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-client-456://authorize/?code=value&state=value"), ClientID));
	TestEqual(TEXT("ClientID with hyphen is extracted"), ClientID, TEXT("client-456"));

	TestFalse(TEXT("different endpoint is rejected"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-client://other?code=value&state=value"), ClientID));
	TestFalse(TEXT("authorize subpath is rejected"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-client://authorize/other?code=value&state=value"), ClientID));
	TestFalse(TEXT("missing query is rejected"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-client://authorize/"), ClientID));
	TestFalse(TEXT("empty query is rejected"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-client://authorize/?"), ClientID));
	TestFalse(TEXT("empty ClientID is rejected"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-://authorize?code=value&state=value"), ClientID));
	TestFalse(TEXT("invalid ClientID character is rejected"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("open-taptap-client.id://authorize?code=value&state=value"), ClientID));
	TestFalse(TEXT("invalid scheme is rejected"),
		FTapDeepLinkHandler::TryParseWindowsProtocolURL(
			TEXT("https://authorize?code=value&state=value"), ClientID));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

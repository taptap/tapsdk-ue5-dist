// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Internal/Http/TapHttpRequest.h"
#include "Internal/Http/TapHttpCommonParams.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapHttpRequestVersionTest,
	"TapSDK.Release.Version.HttpHeaders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapHttpRequestVersionTest::RunTest(const FString&)
{
	const FString ExpectedUA = FString::Printf(TEXT("TapSDK-UE5/%s"), TapCore_VERSION);
	// Inspect a real request without sending it or initializing the SDK.
	const TSharedRef<IHttpRequest> Request = FTapHttpRequest::CreateRequest(
		TEXT("GET"), TEXT("https://example.invalid"), 1.0f);
	TestEqual(TEXT("Unsigned requests use the packaged SDK version"),
		Request->GetHeader(TEXT("User-Agent")), ExpectedUA);
	const auto Headers = FTapHttpCommonParams::GetCommonHeaders(
		TEXT("TestModule"), TEXT("9.8.7-test"), false);
	TestEqual(TEXT("Signed requests use the same SDK UA"),
		Headers.FindRef(TEXT("User-Agent")), ExpectedUA);
	TestEqual(TEXT("Module version stays independent of the SDK UA"),
		Headers.FindRef(TEXT("X-Tap-SDK-Module-Version")), FString(TEXT("9.8.7-test")));
	return true;
}
#endif

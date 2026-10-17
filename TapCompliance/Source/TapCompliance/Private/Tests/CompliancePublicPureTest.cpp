// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "Internal/Http/TapHttp.h"
#include "TapCompliance.h"
#include "TapCompliancePaymentResult.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCompliancePublicAsyncNotInitializedTest,
	"TapCompliance.Public.Pure.AsyncCallbackWhenNotInitialized",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FCompliancePublicAsyncNotInitializedTest::RunTest(const FString& /*Parameters*/)
{
	if (FTapHttp::IsInitialized())
	{
		AddInfo(TEXT("Skip: TapHttp already initialized in this process"));
		return true;
	}

	// This expectation is tied to FTapSdkChecker logging an Error for each failed init check.
	AddExpectedErrorPlain(TEXT("当前应用还未初始化"), EAutomationExpectedErrorFlags::Contains, 2);

	bool bPaymentLimitCalled = false;
	FTapCompliance::CheckPaymentLimit(100, [&bPaymentLimitCalled, this](
		int32 Status, const FString& Title, const FString& Description)
	{
		AddError(TEXT("Uninitialized check must not report a business result"));
	}, [&bPaymentLimitCalled, this](const FString& Message)
	{
		bPaymentLimitCalled = true;
		TestEqual(TEXT("payment check error"), Message, FString(TEXT("TapSDK not initialized")));
	});
	TestTrue(TEXT("payment check callback must be invoked before SDK init"), bPaymentLimitCalled);

	bool bSubmitCalled = false;
	FTapCompliance::SubmitPayment(100, [&bSubmitCalled, this](bool bSuccess, const FString& Message)
	{
		bSubmitCalled = true;
		TestFalse(TEXT("submit payment should fail before SDK init"), bSuccess);
		TestEqual(TEXT("submit payment message"), Message, FString(TEXT("TapSDK not initialized")));
	});
	TestTrue(TEXT("submit payment callback must be invoked before SDK init"), bSubmitCalled);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePaymentResultChannelsTest,
	"TapCompliance.Public.Pure.PaymentResultChannels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePaymentResultChannelsTest::RunTest(const FString&)
{
	int32 Results = 0, Errors = 0;
	int32 Status = -1;
	TFunction<void(int32, const FString&, const FString&)> OnResult =
		[&](int32 Value, const FString&, const FString&) { ++Results; Status = Value; };
	TFunction<void(const FString&)> OnError = [&](const FString&) { ++Errors; };
	TapCompliancePaymentResult::Dispatch(TEXT("{\"status\":0,\"title\":\"restricted\"}"), OnResult, OnError);
	TestEqual(TEXT("Business denial remains a result"), Status, 0);
	TestEqual(TEXT("Denial does not call error"), Errors, 0);
	TapCompliancePaymentResult::Dispatch(TEXT("{\"status\":1}"), OnResult, OnError);
	TestEqual(TEXT("Allowed result is preserved"), Status, 1);
	for (const TCHAR* Invalid : {TEXT("{}"), TEXT("{\"status\":2}"), TEXT("invalid")})
	{
		TapCompliancePaymentResult::Dispatch(Invalid, OnResult, OnError);
	}
	TestEqual(TEXT("Only completed checks call result"), Results, 2);
	TestEqual(TEXT("Malformed responses call error"), Errors, 3);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

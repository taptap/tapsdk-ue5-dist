// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Tests/EnsureScope.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "TapLicense.h"
#include "Internal/TapLicenseInternal.h"
#include "TapLicenseCallbackManager.h"
#include "Internal/Launcher/TapLauncherDesktopImpl.h"
#include "Win64/TapLicenseAnalytics.h"

// ── Test 1: 未调用启动校验时，License API 报告前置条件错误 ────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLicenseAPIsBeforeLauncherTest,
	"TapSDK.License.UTapLicense.APIsBeforeLauncherCheck",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLicenseAPIsBeforeLauncherTest::RunTest(const FString&)
{
	FTapLauncherDesktopImpl::Reset();
	FTapLicenseInternal::SetupCallbacks();

	bool bLicenseCallback = false;
	bool bQueryCallback = false;
	int32 ErrorCallbackCount = 0;
	FDelegateHandle LicenseHandle = FTapLicenseCallbackManager::Get().TestFailed.AddLambda(
		[&]{ bLicenseCallback = true; });
	FDelegateHandle QueryHandle = FTapLicenseCallbackManager::Get().TestQuery.AddLambda(
		[&](ETapLicenseQueryCode, const FTapLicenseDLCResultMap&)
		{ bQueryCallback = true; });
	FDelegateHandle ErrorHandle = FTapLicenseCallbackManager::Get().TestError.AddLambda(
		[&](const FString& Message)
		{
			if (Message.Contains(TEXT("requires UTapLauncher::IsLaunchedFromTapTapPC() to be called first")))
			{
				++ErrorCallbackCount;
			}
		});

	FEnsureScope EnsureScope;
	AddExpectedErrorPlain(TEXT("TapLicense.CheckLicense requires UTapLauncher::IsLaunchedFromTapTapPC() to be called first"),
		EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("TapLicense.QueryDLC requires UTapLauncher::IsLaunchedFromTapTapPC() to be called first"),
		EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("TapLicense.PurchaseDLC requires UTapLauncher::IsLaunchedFromTapTapPC() to be called first"),
		EAutomationExpectedErrorFlags::Contains, 1);

	UTapLicense::CheckLicense(false);
	UTapLicense::QueryDLC({TEXT("sku_001")});
	UTapLicense::PurchaseDLC(TEXT("sku_001"));

	TestEqual(TEXT("Pre-check APIs trigger one ensure each"), EnsureScope.GetCount(), 3);
	TestEqual(TEXT("Pre-check APIs broadcast one diagnostic error each"), ErrorCallbackCount, 3);

	FTapLicenseCallbackManager::Get().TestFailed.Remove(LicenseHandle);
	FTapLicenseCallbackManager::Get().TestQuery.Remove(QueryHandle);
	FTapLicenseCallbackManager::Get().TestError.Remove(ErrorHandle);

	TestFalse(TEXT("Pre-check CheckLicense does not broadcast a normal failure callback"), bLicenseCallback);
	TestFalse(TEXT("Pre-check QueryDLC does not broadcast a normal query callback"), bQueryCallback);
	return true;
}

// ── Test 3: OnDLCOrderChanged 直接广播验证回调绑定 ────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLicenseDLCOrderChangedTest,
	"TapSDK.License.UTapLicense.DLCOrderChangedBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLicenseDLCOrderChangedTest::RunTest(const FString&)
{
	FString GotSku;
	ETapDLCPurchasedCode GotStatus = ETapDLCPurchasedCode::Undefined;
	FDelegateHandle H = FTapLicenseCallbackManager::Get().TestOrder.AddLambda(
		[&](const FString& Sku, ETapDLCPurchasedCode Status)
		{ GotSku = Sku; GotStatus = Status; });

	// Simulate DLL callback dispatch
	FTapLicenseCallbackManager::Get().Order(TEXT("dlc_001"), ETapDLCPurchasedCode::Purchased);

	FTapLicenseCallbackManager::Get().TestOrder.Remove(H);

	TestEqual(TEXT("OnDLCOrderChanged sku"), GotSku, FString(TEXT("dlc_001")));
	TestEqual(TEXT("OnDLCOrderChanged status"), GotStatus, ETapDLCPurchasedCode::Purchased);
	return true;
}

// ── Test 4: OnLicenseSuccess 直接广播验证回调绑定 ─────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLicenseSuccessBindingTest,
	"TapSDK.License.UTapLicense.LicenseSuccessBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLicenseSuccessBindingTest::RunTest(const FString&)
{
	bool bFired = false;
	FDelegateHandle H = FTapLicenseCallbackManager::Get().TestSuccess.AddLambda([&]{ bFired = true; });

	FTapLicenseCallbackManager::Get().Success();

	FTapLicenseCallbackManager::Get().TestSuccess.Remove(H);

	TestTrue(TEXT("OnLicenseSuccess binding works"), bFired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLicenseAnalyticsMetadataTest,
	"TapSDK.License.Analytics.OperationMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLicenseAnalyticsMetadataTest::RunTest(const FString&)
{
	const TapLicenseAnalytics::FOperation License = TapLicenseAnalytics::MakeCheckLicenseOperation(true);
	TestEqual(TEXT("Forced CheckLicense function name"), License.FuncName, FString(TEXT("checkLicense")));
	TestEqual(TEXT("Forced CheckLicense license type"), License.Props.FindRef(TEXT("license_type")), FString(TEXT("tap_license")));
	const TapLicenseAnalytics::FOperation LocalLicense = TapLicenseAnalytics::MakeCheckLicenseOperation(false);
	TestEqual(TEXT("Local CheckLicense license type"), LocalLicense.Props.FindRef(TEXT("license_type")), FString(TEXT("local_license")));
	const TapLicenseAnalytics::FOperation Query = TapLicenseAnalytics::MakeQueryDLCOperation({TEXT("dlc_001"), TEXT("dlc_002")});
	TestEqual(TEXT("QueryDLC function name"), Query.FuncName, FString(TEXT("queryDLC")));
	TestEqual(TEXT("QueryDLC SKU list"), Query.Props.FindRef(TEXT("sku_ids")), FString(TEXT("dlc_001,dlc_002")));
	const TapLicenseAnalytics::FOperation Purchase = TapLicenseAnalytics::MakePurchaseDLCOperation(TEXT("dlc_003"));
	TestEqual(TEXT("PurchaseDLC function name"), Purchase.FuncName, FString(TEXT("purchaseDLC")));
	TestEqual(TEXT("PurchaseDLC SKU"), Purchase.Props.FindRef(TEXT("sku_id")), FString(TEXT("dlc_003")));
	const FString FirstSession = TapLicenseAnalytics::NewSessionId();
	const FString SecondSession = TapLicenseAnalytics::NewSessionId();
	TestFalse(TEXT("Session ID is not empty"), FirstSession.IsEmpty());
	TestNotEqual(TEXT("Each operation gets a unique session ID"), FirstSession, SecondSession);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

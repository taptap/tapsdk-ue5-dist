// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapLicenseTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapLicenseDLCStatusConversionTest,
	"TapSDK.License.Types.DLCStatusConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLicenseDLCStatusConversionTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Native not-purchased status maps to UE enum"),
		TapLicenseDLCStatus::FromNativeValue(0), ETapDLCPurchasedCode::NotPurchased);
	TestEqual(TEXT("Native purchased status maps to UE enum"),
		TapLicenseDLCStatus::FromNativeValue(1), ETapDLCPurchasedCode::Purchased);
	TestEqual(TEXT("Native error status maps to UE enum"),
		TapLicenseDLCStatus::FromNativeValue(-1), ETapDLCPurchasedCode::ReturnError);
	TestEqual(TEXT("Unknown native status maps to undefined"),
		TapLicenseDLCStatus::FromNativeValue(42), ETapDLCPurchasedCode::Undefined);
	TestEqual(TEXT("UE error enum converts back to native -1"),
		TapLicenseDLCStatus::ToNativeValue(ETapDLCPurchasedCode::ReturnError), -1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/TapSdkOptionSerializerRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TapComplianceOptions.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapComplianceOptionsSerializationTest,
	"TapSDK.Compliance.Options.SerializeForNativeInit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapComplianceOptionsSerializationTest::RunTest(const FString&)
{
	FTapComplianceOptions ComplianceOptions;
	ComplianceOptions.bShowSwitchAccount = true;
	ComplianceOptions.bUseAgeRange = false;

	FTapTapSdkOptions Config;
	FTapSdkConfigState::SetModuleOptions(
		Config,
		{FTapSdkOption::Make<FTapComplianceOptions>(ComplianceOptions)});

	TArray<FTapSerializedSdkModuleOption> SerializedOptions;
	FTapSdkOptionSerializerRegistry::Get().SerializeOptions(Config, SerializedOptions);
	TestEqual(TEXT("One Compliance option should be serialized"), SerializedOptions.Num(), 1);
	if (SerializedOptions.Num() != 1)
	{
		return false;
	}

	TestEqual(TEXT("iOS native option class should stay internal and deterministic"),
		SerializedOptions[0].NativeClassName, FString(TEXT("TapTapComplianceOptions")));

	TSharedPtr<FJsonObject> Json;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SerializedOptions[0].Json);
	TestTrue(TEXT("Serialized Compliance option should be valid JSON"),
		FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid());
	if (Json.IsValid())
	{
		TestTrue(TEXT("showSwitchAccount should be forwarded"), Json->GetBoolField(TEXT("showSwitchAccount")));
		TestFalse(TEXT("useAgeRange should be forwarded"), Json->GetBoolField(TEXT("useAgeRange")));
	}
	return true;
}

#if !UE_BUILD_SHIPPING && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "Desktop/ComplianceState.h"
#include "Internal/TapComplianceTestOptionsScope.h"
#include "Misc/ScopeExit.h"
#include "Platform/TapCompliancePlatformDesktop.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapComplianceDesktopInitOptionsTest,
	"TapSDK.Compliance.Options.ApplyForDesktopInit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapComplianceDesktopInitOptionsTest::RunTest(const FString&)
{
	FComplianceState& State = FComplianceState::Get();
	const bool bOriginalUseAgeRange = State.bUseAgeRange;
	const bool bOriginalShowSwitchAccount = State.bShowSwitchAccount;
	ON_SCOPE_EXIT
	{
		State.bUseAgeRange = bOriginalUseAgeRange;
		State.bShowSwitchAccount = bOriginalShowSwitchAccount;
	};

	FTapCompliancePlatformDesktop Platform;
	FTapComplianceOptions Options;
	Options.bUseAgeRange = false;
	Options.bShowSwitchAccount = true;
	Platform.ApplyInitOptions(Options);
	TestFalse(TEXT("Desktop init applies useAgeRange=false"), State.bUseAgeRange);
	TestTrue(TEXT("Desktop init applies showSwitchAccount=true"), State.bShowSwitchAccount);
	Platform.ApplyInitOptions(FTapComplianceOptions());
	TestTrue(TEXT("Default init restores useAgeRange=true"), State.bUseAgeRange);
	TestFalse(TEXT("Default init restores showSwitchAccount=false"), State.bShowSwitchAccount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapComplianceScopedTestOptionsTest,
	"TapSDK.Compliance.Options.ScopedTestOverrides",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapComplianceScopedTestOptionsTest::RunTest(const FString&)
{
	FComplianceState& State = FComplianceState::Get();
	const bool bOriginalUseAgeRange = State.bUseAgeRange;
	const bool bOriginalShowSwitchAccount = State.bShowSwitchAccount;
	const FComplianceVerification OriginalVerification = State.Verification;
	ON_SCOPE_EXIT
	{
		State.bUseAgeRange = bOriginalUseAgeRange;
		State.bShowSwitchAccount = bOriginalShowSwitchAccount;
		State.Verification = OriginalVerification;
	};
	State.bUseAgeRange = true;
	State.bShowSwitchAccount = false;
	State.Verification.AgeLimit = 16;
	{
		FTapComplianceTestOptionsScope Outer(false, true);
		TestEqual(TEXT("Disabled age exposure returns -1"), State.GetExposedAgeRange(), -1);
		TestTrue(TEXT("Switch-account override reaches Desktop state"), State.bShowSwitchAccount);
		{
			FTapComplianceTestOptionsScope Inner(true, TOptional<bool>());
			TestEqual(TEXT("Enabled age exposure returns the actual age"), State.GetExposedAgeRange(), 16);
			TestTrue(TEXT("Unspecified switch-account option stays enabled"), State.bShowSwitchAccount);
		}
		TestEqual(TEXT("Nested scope restores the outer age override"), State.GetExposedAgeRange(), -1);
	}
	TestTrue(TEXT("Scope restores the initial age option"), State.bUseAgeRange);
	TestFalse(TEXT("Scope restores the initial switch-account option"), State.bShowSwitchAccount);
	return true;
}

#endif

#endif

// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapTapSdkOptions.h"
#include "Internal/Http/TapHttpTypes.h"
#include "Internal/TapSdkConfigState.h"
#include "Misc/ScopeExit.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapTapSdkOptionsDefaultsToProductionTest,
	"TapSDK.Core.Config.DefaultsToProduction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapTapSdkOptionsDefaultsToProductionTest::RunTest(const FString&)
{
	const FTapTapSdkOptions OriginalConfig = FTapSdkConfigState::GetCurrentConfig();
	ON_SCOPE_EXIT
	{
		FTapSdkConfigState::SetCurrentConfig(OriginalConfig);
	};

	const FTapTapSdkOptions Config;
	TestEqual(TEXT("Core options should expose the Unity-aligned reflected name"),
		FTapTapSdkOptions::StaticStruct()->GetPathName(), FString(TEXT("/Script/TapCore.TapTapSdkOptions")));
	const FCoreRedirectObjectName RedirectedName = FCoreRedirects::GetRedirectedName(
		ECoreRedirectFlags::Type_Struct, FCoreRedirectObjectName(TEXT("/Script/TapCore.TapConfig")));
	TestEqual(TEXT("Existing Blueprint struct references should resolve to the renamed options"),
		RedirectedName.ToString(), FTapTapSdkOptions::StaticStruct()->GetPathName());
	TestFalse(TEXT("TapSDK should use the production environment by default"),
		FTapSdkConfigState::IsRNDEnvironment());
	TestEqual(TEXT("FTapTapSdkOptions should default to landscape like Unity"), Config.ScreenOrientation, 1);

	const FTapTapEventOptions& EventOptions = FTapSdkConfigState::GetTapEventOptions(Config);
	TestTrue(TEXT("TapTap Event should be enabled by default"), EventOptions.bEnableTapTapEvent);
	TestTrue(TEXT("Automatic IAP events should be enabled by default"), EventOptions.bEnableAutoIAPEvent);

	FTapTapEventOptions CustomEventOptions;
	CustomEventOptions.bEnableTapTapEvent = false;
	TArray<FTapSdkOption> OtherOptions;
	OtherOptions.Add(FTapSdkOption::Make<FTapTapEventOptions>(CustomEventOptions));

	FTapTapSdkOptions Configured = Config;
	FTapSdkConfigState::SetModuleOptions(Configured, OtherOptions);
	TestFalse(TEXT("TapTap Event options should be resolved from the options array"),
		FTapSdkConfigState::GetTapEventOptions(Configured).bEnableTapTapEvent);
	TestEqual(TEXT("Additional options should be stored without exposing them on FTapTapSdkOptions"),
		FTapSdkConfigState::GetModuleOptions(Configured).Num(), 1);

	FTapSdkConfigState::SetCurrentConfig(Config);
	FTapSdkConfigState::SetPreferredLanguage(ETapLanguage::en);
	TestEqual(TEXT("Runtime language updates should change the SDK-owned state"),
		FTapSdkConfigState::GetCurrentConfig().PreferredLanguage, ETapLanguage::en);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapCoreMergedTypesTest,
	"TapSDK.Core.Config.MergedTypesResolveFromOldModule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapCoreMergedTypesTest::RunTest(const FString&)
{
	UEnum* Enums[] = { StaticEnum<ETapRegion>(), StaticEnum<ETapLanguage>(),
		StaticEnum<ETapHttpMethod>(), StaticEnum<ETapHttpErrorType>() };
	for (UEnum* Enum : Enums)
	{
		const FString OldPath = TEXT("/Script/TapBasic.") + Enum->GetName();
		TestEqual(TEXT("Merged enum belongs to TapCore"), Enum->GetOutermost()->GetName(),
			FString(TEXT("/Script/TapCore")));
		TestEqual(TEXT("Old enum reference resolves to the same reflected type"),
			LoadObject<UEnum>(nullptr, *OldPath), Enum);
	}
	UScriptStruct* Structs[] = { FTapHttpErrorData::StaticStruct(), FTapHttpServerResponse::StaticStruct() };
	for (UScriptStruct* Struct : Structs)
	{
		const FString OldPath = TEXT("/Script/TapBasic.") + Struct->GetName();
		TestEqual(TEXT("Merged struct belongs to TapCore"), Struct->GetOutermost()->GetName(),
			FString(TEXT("/Script/TapCore")));
		TestEqual(TEXT("Old struct reference resolves to the same reflected type"),
			LoadObject<UScriptStruct>(nullptr, *OldPath), Struct);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

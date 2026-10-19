// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/ConfigCacheIni.h"
#include "Internal/TapDBConstants.h"
#include "TapDBDeviceInfo.h"
#include "Internal/TapDBIdentity.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapDBDeviceInfoGameVersionTest,
	"TapDB.DeviceInfo.GameVersion.UsesProjectVersionByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapDBDeviceInfoGameVersionTest::RunTest(const FString& /*Parameters*/)
{
	FString ProjectVersion;
	const bool bHasProjectVersion = GConfig && GConfig->GetString(
		TEXT("/Script/EngineSettings.GeneralProjectSettings"),
		TEXT("ProjectVersion"),
		ProjectVersion,
		GGameIni);
	TestTrue(TEXT("UE project version should be available"), bHasProjectVersion && !ProjectVersion.IsEmpty());

	const FTapDBIdentity Identity;
	const TSharedPtr<FJsonObject> DefaultProperties = FTapDBDeviceInfo::CollectBasicProperties(
		Identity, FString(), FString(), TEXT("test-sdk"));
	TestEqual(TEXT("Empty GameVersion should use the UE project version"),
		DefaultProperties->GetStringField(TapDBConstants::PROP_APP_VERSION), ProjectVersion);

	const TSharedPtr<FJsonObject> ExplicitProperties = FTapDBDeviceInfo::CollectBasicProperties(
		Identity, TEXT("9.8.7"), FString(), TEXT("test-sdk"));
	TestEqual(TEXT("Explicit GameVersion should take precedence"),
		ExplicitProperties->GetStringField(TapDBConstants::PROP_APP_VERSION), FString(TEXT("9.8.7")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "Dom/JsonObject.h"
#include "Internal/TapSdkConfigState.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Internal/Platform/TapOpenlog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapOpenlogGameVersionTest,
	"TapSDK.Core.OpenLog.GameVersion.UsesApplicationVersion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapOpenlogGameVersionTest::RunTest(const FString&)
{
	if (!GConfig)
	{
		AddError(TEXT("Config cache is required to test the application version"));
		return false;
	}

	const TCHAR* Section = TEXT("/Script/EngineSettings.GeneralProjectSettings");
	FString OriginalProjectVersion;
	const bool bHadProjectVersion = GConfig->GetString(Section, TEXT("ProjectVersion"), OriginalProjectVersion, GGameIni);
	const FTapTapSdkOptions OriginalConfig = FTapSdkConfigState::GetCurrentConfig();
	FString OriginalLocale;
	{
		FScopeLock Lock(&FTapOpenlog::DynamicVarsMutex);
		OriginalLocale = FTapOpenlog::CurrentSdkLocale;
	}
	ON_SCOPE_EXIT
	{
		if (bHadProjectVersion)
		{
			GConfig->SetString(Section, TEXT("ProjectVersion"), *OriginalProjectVersion, GGameIni);
		}
		else
		{
			GConfig->RemoveKey(Section, TEXT("ProjectVersion"), GGameIni);
		}
		FTapSdkConfigState::SetCurrentConfig(OriginalConfig);
		FScopeLock Lock(&FTapOpenlog::DynamicVarsMutex);
		FTapOpenlog::CurrentSdkLocale = OriginalLocale;
	};

	GConfig->SetString(Section, TEXT("ProjectVersion"), TEXT("1.2.3-project"), GGameIni);
	FTapTapSdkOptions Config;
	Config.GameVersion = TEXT("9.8.7-custom");
	FTapSdkConfigState::SetCurrentConfig(Config);
	TSharedRef<FJsonObject> Common = MakeShared<FJsonObject>();
	FTapOpenlog::CollectGeneralParameters(Common);
	TestEqual(TEXT("OpenLog should use ProjectVersion even when SDK GameVersion is supplied"),
		Common->GetStringField(TEXT("app_version")), FString(TEXT("1.2.3-project")));

	Config.GameVersion.Empty();
	FTapSdkConfigState::SetCurrentConfig(Config);
	FTapOpenlog::CollectGeneralParameters(Common);
	TestEqual(TEXT("Empty GameVersion should not change the OpenLog application version"),
		Common->GetStringField(TEXT("app_version")), FString(TEXT("1.2.3-project")));

	GConfig->RemoveKey(Section, TEXT("ProjectVersion"), GGameIni);
	Config.GameVersion = TEXT("9.8.7-custom");
	FTapSdkConfigState::SetCurrentConfig(Config);
	FTapOpenlog::CollectGeneralParameters(Common);
	TestEqual(TEXT("Missing ProjectVersion must not be replaced by SDK GameVersion or the UE build version"),
		Common->GetStringField(TEXT("app_version")), FString());
	return true;
}

#endif

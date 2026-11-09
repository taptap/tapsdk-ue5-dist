// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "Dom/JsonObject.h"
#include "Internal/Platform/TapOpenlog.h"
#include "Internal/TapSdkConfigState.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapOpenlogDataDirectoryTest,
	"TapSDK.Core.OpenLog.DataDirectory.PreparesBeforeNativeStartup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapOpenlogDataDirectoryTest::RunTest(const FString&)
{
	const FTapTapSdkOptions OriginalConfig = FTapSdkConfigState::GetCurrentConfig();
	FTapTapSdkOptions Config;
	Config.ClientID = TEXT("Automation-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString DataDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("OpenlogData_") + Config.ClientID);
	IFileManager& Files = IFileManager::Get();
	ON_SCOPE_EXIT
	{
		Files.Delete(*DataDir);
		Files.DeleteDirectory(*DataDir, false, true);
		FTapSdkConfigState::SetCurrentConfig(OriginalConfig);
	};
	FTapSdkConfigState::SetCurrentConfig(Config);
	const TSharedRef<FJsonObject> Common = MakeShared<FJsonObject>();
	TestFalse(TEXT("First launch starts without the data directory"), Files.DirectoryExists(*DataDir));
	const FString Json = FTapOpenlog::BuildStartConfig(Common);
	TestTrue(TEXT("Building start config creates the directory"), Files.DirectoryExists(*DataDir));
	TSharedPtr<FJsonObject> StartConfig;
	if (!TestTrue(TEXT("Prepared directory produces valid start config"),
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), StartConfig) && StartConfig.IsValid()))
	{
		return false;
	}
	TestTrue(TEXT("Native data_dir points to the created directory"),
		FPaths::IsSamePath(StartConfig->GetStringField(TEXT("data_dir")), DataDir));
	TestFalse(TEXT("Native data_dir is an absolute path"),
		FPaths::IsRelative(StartConfig->GetStringField(TEXT("data_dir"))));
	TestFalse(TEXT("Existing directory allows repeated preparation"), FTapOpenlog::BuildStartConfig(Common).IsEmpty());
	if (!TestTrue(TEXT("Remove empty directory to simulate a file collision"), Files.DeleteDirectory(*DataDir, false, false)) ||
		!TestTrue(TEXT("Create a file at the directory path"), FFileHelper::SaveStringToFile(TEXT("occupied"), *DataDir)))
	{
		return false;
	}
	TestTrue(TEXT("Directory creation failure prevents producing native start config"),
		FTapOpenlog::BuildStartConfig(Common).IsEmpty());
	TestTrue(TEXT("The conflicting file remains intact"), Files.FileExists(*DataDir));
	TestTrue(TEXT("Remove the conflicting file"), Files.Delete(*DataDir));
	TestFalse(TEXT("Preparation can retry after the obstruction is removed"), FTapOpenlog::BuildStartConfig(Common).IsEmpty());
	TestTrue(TEXT("Retry creates the directory"), Files.DirectoryExists(*DataDir));
	return true;
}

#endif

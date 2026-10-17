// Copyright TapTap. All Rights Reserved.

#include "Init/TapComplianceInitTask.h"
#include "TapComplianceModule.h"
#include "TapComplianceOptions.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "Internal/TapSdkConfigState.h"
#include "Platform/ITapCompliancePlatform.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Desktop/TapComplianceTracker.h"
#endif

FTapTaskConfig FTapComplianceInitTask::TaskConfig;
FTapComplianceInitTask FTapComplianceInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapComplianceInitTask> FTapComplianceInitTask::AutoRegister;

namespace
{
	struct FTaskConfigInitializer
	{
		FTaskConfigInitializer()
		{
			// Unity ComplianceInitTask.Order=12; UE priority 12 keeps init tracking after TapCore/OpenLog and before TapLogin.
			FTapComplianceInitTask::WithPriority(12);
			FTapComplianceInitTask::CanRunInBackground(false);
		}
	};
	static FTaskConfigInitializer GTaskConfigInit;
}

void FTapComplianceInitTask::Execute(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapCompliance, Log, TEXT("TapCompliance Initialization Started"));
	const FTapComplianceOptions* ConfiguredOptions = FTapSdkConfigState::FindModuleOption<FTapComplianceOptions>(Config);
	const FTapComplianceOptions DefaultOptions;
	const FTapComplianceOptions& Options = ConfiguredOptions ? *ConfiguredOptions : DefaultOptions;
	if (TSharedPtr<ITapCompliancePlatform> Platform = FTapCompliancePlatformFactory::CreatePlatform())
	{
		Platform->ApplyInitOptions(Options);
	}

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	FTapComplianceTracker::Get().TrackInit();
#endif

	UE_LOG(LogTapCompliance, Log, TEXT("TapCompliance Initialization Completed"));
}

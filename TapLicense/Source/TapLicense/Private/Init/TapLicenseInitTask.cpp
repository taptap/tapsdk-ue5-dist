// Copyright TapTap. All Rights Reserved.
#include "Init/TapLicenseInitTask.h"
#include "TapLicense.h"
#include "Internal/TapLicenseInternal.h"
#include "TapLicenseModule.h"
#include "Internal/Init/TapInitTaskRegistry.h"

FTapTaskConfig FTapLicenseInitTask::TaskConfig;
FTapLicenseInitTask FTapLicenseInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapLicenseInitTask> FTapLicenseInitTask::AutoRegister;

namespace
{
	struct FTaskConfigInitializer
	{
		FTaskConfigInitializer()
		{
			// Priority 25: after TapLogin (20), before TapCloudSave (30)
			// UTapLicense::Initialize() only registers callbacks;
			// actual DLL calls require UTapLauncher::IsPassedLauncherCheck() == true.
			FTapLicenseInitTask::WithPriority(25);
			FTapLicenseInitTask::CanRunInBackground(false);
		}
	};
	static FTaskConfigInitializer GTaskConfigInit;
}

void FTapLicenseInitTask::Execute(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapLicense, Log, TEXT("TapLicense Initialization Started"));
	FTapLicenseInternal::SetupCallbacks();
	FTapLicenseInternal::Initialize();
	UE_LOG(LogTapLicense, Log, TEXT("TapLicense Initialization Completed"));
}

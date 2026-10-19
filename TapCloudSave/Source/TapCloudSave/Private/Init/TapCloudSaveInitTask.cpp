// Copyright TapTap. All Rights Reserved.

#include "Init/TapCloudSaveInitTask.h"
#include "TapCloudSaveModule.h"
#include "TapCloudSave.h"
#include "Platform/ITapCloudSavePlatform.h"
#include "Internal/Init/TapInitTaskRegistry.h"

FTapTaskConfig FTapCloudSaveInitTask::TaskConfig;
FTapCloudSaveInitTask FTapCloudSaveInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapCloudSaveInitTask> FTapCloudSaveInitTask::AutoRegister;

namespace
{
	struct FTaskConfigInitializer
	{
		FTaskConfigInitializer()
		{
			// CloudSave 依赖登录态，优先级在 TapLogin (20) 之后
			FTapCloudSaveInitTask::WithPriority(30);
			FTapCloudSaveInitTask::CanRunInBackground(false);
		}
	};
	static FTaskConfigInitializer GTaskConfigInit;
}

void FTapCloudSaveInitTask::Execute(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapCloudSave, Log, TEXT("========================================"));
	UE_LOG(LogTapCloudSave, Log, TEXT("TapCloudSave Initialization Started"));
	UE_LOG(LogTapCloudSave, Log, TEXT("Version: %s"), *UTapCloudSave::GetVersion());
	UE_LOG(LogTapCloudSave, Log, TEXT("========================================"));

	if (auto P = FTapCloudSavePlatformFactory::CreatePlatform())
	{
		P->Initialize();
	}
	else
	{
		UE_LOG(LogTapCloudSave, Warning, TEXT("TapCloudSave platform not available; skipping init"));
	}

	UE_LOG(LogTapCloudSave, Log, TEXT("TapCloudSave Initialization Completed"));
}

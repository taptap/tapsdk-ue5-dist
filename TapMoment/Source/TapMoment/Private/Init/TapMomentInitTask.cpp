// Copyright TapTap. All Rights Reserved.
#include "Init/TapMomentInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapMomentModule.h"
#include "Platform/ITapMomentPlatform.h"

FTapTaskConfig FTapMomentInitTask::TaskConfig;
FTapMomentInitTask FTapMomentInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapMomentInitTask> FTapMomentInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 25: 在 TapLogin(20) 之后
            FTapMomentInitTask::WithPriority(25);
            FTapMomentInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapMomentInitTask::Execute(const FTapTapSdkOptions& Config)
{
    UE_LOG(LogTapMoment, Log, TEXT("TapMoment InitTask started"));
    if (auto P = FTapMomentPlatformFactory::CreatePlatform())
    {
        P->Initialize(Config);
    }
    UE_LOG(LogTapMoment, Log, TEXT("TapMoment InitTask completed"));
}

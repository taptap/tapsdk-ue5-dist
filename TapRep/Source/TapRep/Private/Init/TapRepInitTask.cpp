// Copyright TapTap. All Rights Reserved.
#include "Init/TapRepInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapRepModule.h"
#include "Platform/ITapRepPlatform.h"

FTapTaskConfig FTapRepInitTask::TaskConfig;
FTapRepInitTask FTapRepInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapRepInitTask> FTapRepInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 18: 对齐 Unity TapRepInitTask.Order = 18
            FTapRepInitTask::WithPriority(18);
            FTapRepInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapRepInitTask::Execute(const FTapTapSdkOptions& Config)
{
    UE_LOG(LogTapRep, Log, TEXT("TapRep InitTask started"));
    if (auto P = FTapRepPlatformFactory::CreatePlatform())
    {
        P->Initialize(Config);
    }
    UE_LOG(LogTapRep, Log, TEXT("TapRep InitTask completed"));
}

// Copyright TapTap. All Rights Reserved.
#include "Init/TapShareInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapShareModule.h"
#include "Platform/ITapSharePlatform.h"

FTapTaskConfig FTapShareInitTask::TaskConfig;
FTapShareInitTask FTapShareInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapShareInitTask> FTapShareInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 19: 分享无 Unity InitTask 参照，取社交模块簇内空闲位（rep=18 之后）
            FTapShareInitTask::WithPriority(19);
            FTapShareInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapShareInitTask::Execute(const FTapTapSdkOptions& Config)
{
    UE_LOG(LogTapShare, Log, TEXT("TapShare InitTask started"));
    if (auto P = FTapSharePlatformFactory::CreatePlatform())
    {
        P->Initialize(Config);
    }
    UE_LOG(LogTapShare, Log, TEXT("TapShare InitTask completed"));
}

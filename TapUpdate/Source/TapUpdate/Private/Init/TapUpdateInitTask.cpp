// Copyright TapTap. All Rights Reserved.
#include "Init/TapUpdateInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapUpdateModule.h"


FTapTaskConfig FTapUpdateInitTask::TaskConfig;
FTapUpdateInitTask FTapUpdateInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapUpdateInitTask> FTapUpdateInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 14: 对齐 Unity TapUpdateInitTask.Order = 14
            FTapUpdateInitTask::WithPriority(14);
            FTapUpdateInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapUpdateInitTask::Execute(const FTapTapSdkOptions& Config)
{
    // Android 侧初始化在 UPL gameActivityOnCreateAdditions 完成（EngineBridge.init + registerService），
    // UE 侧无需额外操作。
    UE_LOG(LogTapUpdate, Log, TEXT("TapUpdate InitTask completed"));
}

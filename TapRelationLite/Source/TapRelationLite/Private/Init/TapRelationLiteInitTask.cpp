// Copyright TapTap. All Rights Reserved.
#include "Init/TapRelationLiteInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapRelationLiteModule.h"
#include "Platform/ITapRelationLitePlatform.h"

FTapTaskConfig FTapRelationLiteInitTask::TaskConfig;
FTapRelationLiteInitTask FTapRelationLiteInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapRelationLiteInitTask> FTapRelationLiteInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 27: 在 TapRelation(26) 之后，对齐 Unity RelationLiteInitTask.Order=105
            FTapRelationLiteInitTask::WithPriority(27);
            FTapRelationLiteInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapRelationLiteInitTask::Execute(const FTapTapSdkOptions& Config)
{
    UE_LOG(LogTapRelationLite, Log, TEXT("TapRelationLite InitTask started"));
    if (auto P = FTapRelationLitePlatformFactory::CreatePlatform())
    {
        P->Initialize(Config);
    }
    UE_LOG(LogTapRelationLite, Log, TEXT("TapRelationLite InitTask completed"));
}

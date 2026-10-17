// Copyright TapTap. All Rights Reserved.
#include "Init/TapRelationInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapRelationModule.h"
#include "Platform/ITapRelationPlatform.h"

FTapTaskConfig FTapRelationInitTask::TaskConfig;
FTapRelationInitTask FTapRelationInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapRelationInitTask> FTapRelationInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 26: 在 TapMoment(25) 之后，对齐 Unity RelationInitTask.Order=104
            FTapRelationInitTask::WithPriority(26);
            FTapRelationInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapRelationInitTask::Execute(const FTapTapSdkOptions& Config)
{
    UE_LOG(LogTapRelation, Log, TEXT("TapRelation InitTask started"));
    if (auto P = FTapRelationPlatformFactory::CreatePlatform())
    {
        P->Initialize(Config);
    }
    UE_LOG(LogTapRelation, Log, TEXT("TapRelation InitTask completed"));
}

// Copyright TapTap. All Rights Reserved.
#include "Init/TapReviewInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapReviewModule.h"

FTapTaskConfig FTapReviewInitTask::TaskConfig;
FTapReviewInitTask FTapReviewInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapReviewInitTask> FTapReviewInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 17: 对齐 Unity TapReviewInitTask.Order = 17
            FTapReviewInitTask::WithPriority(17);
            FTapReviewInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapReviewInitTask::Execute(const FTapTapSdkOptions& Config)
{
    // Android 侧初始化在 UPL gameActivityOnCreateAdditions 完成。
    UE_LOG(LogTapReview, Log, TEXT("TapReview InitTask completed"));
}

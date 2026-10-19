// Copyright TapTap. All Rights Reserved.
#include "Init/TapAchievementInitTask.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "TapAchievementModule.h"
#include "TapAchievementOptions.h"
#include "Platform/ITapAchievementPlatform.h"
#include "Internal/TapSdkConfigState.h"

FTapTaskConfig FTapAchievementInitTask::TaskConfig;
FTapAchievementInitTask FTapAchievementInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapAchievementInitTask> FTapAchievementInitTask::AutoRegister;

namespace
{
    struct FTaskConfigInitializer
    {
        FTaskConfigInitializer()
        {
            // Priority 28: after TapLogin (20), before TapCloudSave (30)
            FTapAchievementInitTask::WithPriority(28);
            FTapAchievementInitTask::CanRunInBackground(false);
        }
    };
    static FTaskConfigInitializer GTaskConfigInit;
}

void FTapAchievementInitTask::Execute(const FTapTapSdkOptions& Config)
{
    UE_LOG(LogTapAchievement, Log, TEXT("TapAchievement Initialization Started"));

    // 经工厂分发：Desktop → HTTP 初始化；Android/iOS → registerService + registerCallback。
    if (TSharedPtr<ITapAchievementPlatform> P = FTapAchievementPlatformFactory::CreatePlatform())
    {
        P->Initialize(Config);
        // Android receives enableToast through native initialization options.
#if !PLATFORM_ANDROID
        const FTapAchievementOptions* ConfiguredOptions = FTapSdkConfigState::FindModuleOption<FTapAchievementOptions>(Config);
        const FTapAchievementOptions DefaultOptions;
        const FTapAchievementOptions& Options = ConfiguredOptions ? *ConfiguredOptions : DefaultOptions;
        P->SetToastEnable(Options.bEnableToast);
#endif
    }
    UE_LOG(LogTapAchievement, Log, TEXT("TapAchievement Initialization Completed"));
}

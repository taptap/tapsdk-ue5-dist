// Copyright TapTap. All Rights Reserved.
#include "TapAchievementModule.h"
#include "Modules/ModuleManager.h"
#include "Platform/ITapAchievementPlatform.h"
#include "TapAchievementCallbackManager.h"
#include "TapAchievementOptions.h"
#include "Internal/TapSdkOptionSerializerRegistry.h"
#include "Dom/JsonObject.h"

DEFINE_LOG_CATEGORY(LogTapAchievement);

void FTapAchievementModule::StartupModule()
{
    FTapSdkOptionSerializerRegistry::Get().RegisterSerializer(
        FTapAchievementOptions::StaticStruct(),
        TEXT("TapTapAchievementOptions"),
        [](const FTapSdkOption& Option, const TSharedRef<FJsonObject>& Json)
        {
            if (const FTapAchievementOptions* AchievementOptions = Option.GetPtr<FTapAchievementOptions>())
            {
                Json->SetBoolField(TEXT("enableToast"), AchievementOptions->bEnableToast);
            }
        });
}
void FTapAchievementModule::ShutdownModule()
{
    FTapSdkOptionSerializerRegistry::Get().UnregisterSerializer(FTapAchievementOptions::StaticStruct());
    FTapAchievementPlatformFactory::Reset();
    FTapAchievementCallbackManager::Get().Reset();
}

IMPLEMENT_MODULE(FTapAchievementModule, TapAchievement)

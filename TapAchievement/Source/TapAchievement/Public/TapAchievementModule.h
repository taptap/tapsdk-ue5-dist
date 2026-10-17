// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPACHIEVEMENT_API DECLARE_LOG_CATEGORY_EXTERN(LogTapAchievement, Log, All);

class FTapAchievementModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FTapAchievementModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FTapAchievementModule>("TapAchievement");
    }
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("TapAchievement");
    }
};

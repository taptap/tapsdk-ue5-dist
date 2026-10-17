// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPLEADERBOARD_API DECLARE_LOG_CATEGORY_EXTERN(LogTapLeaderboard, Log, All);

class FTapLeaderboardModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FTapLeaderboardModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FTapLeaderboardModule>("TapLeaderboard");
    }
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("TapLeaderboard");
    }
};

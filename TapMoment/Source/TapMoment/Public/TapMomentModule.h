// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPMOMENT_API DECLARE_LOG_CATEGORY_EXTERN(LogTapMoment, Log, All);

class FTapMomentModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FTapMomentModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FTapMomentModule>("TapMoment");
    }
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("TapMoment");
    }
};

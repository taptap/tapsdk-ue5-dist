// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPUPDATE_API DECLARE_LOG_CATEGORY_EXTERN(LogTapUpdate, Log, All);

class FTapUpdateModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FTapUpdateModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FTapUpdateModule>("TapUpdate");
    }
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("TapUpdate");
    }
};

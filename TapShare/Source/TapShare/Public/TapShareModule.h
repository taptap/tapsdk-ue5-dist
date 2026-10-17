// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPSHARE_API DECLARE_LOG_CATEGORY_EXTERN(LogTapShare, Log, All);

class FTapShareModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FTapShareModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FTapShareModule>("TapShare");
    }
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("TapShare");
    }
};

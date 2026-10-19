// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPREP_API DECLARE_LOG_CATEGORY_EXTERN(LogTapRep, Log, All);

class FTapRepModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FTapRepModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FTapRepModule>("TapRep");
    }
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("TapRep");
    }
};

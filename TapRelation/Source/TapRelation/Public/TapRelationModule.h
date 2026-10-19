// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPRELATION_API DECLARE_LOG_CATEGORY_EXTERN(LogTapRelation, Log, All);

class FTapRelationModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    static inline FTapRelationModule& Get() { return FModuleManager::LoadModuleChecked<FTapRelationModule>("TapRelation"); }
    static inline bool IsAvailable() { return FModuleManager::Get().IsModuleLoaded("TapRelation"); }
};

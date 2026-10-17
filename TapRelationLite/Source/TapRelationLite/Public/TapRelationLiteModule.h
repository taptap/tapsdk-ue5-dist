// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPRELATIONLITE_API DECLARE_LOG_CATEGORY_EXTERN(LogTapRelationLite, Log, All);

class FTapRelationLiteModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    static inline FTapRelationLiteModule& Get() { return FModuleManager::LoadModuleChecked<FTapRelationLiteModule>("TapRelationLite"); }
    static inline bool IsAvailable() { return FModuleManager::Get().IsModuleLoaded("TapRelationLite"); }
};

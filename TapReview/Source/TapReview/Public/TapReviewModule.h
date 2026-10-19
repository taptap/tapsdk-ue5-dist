// Copyright TapTap. All Rights Reserved.
#pragma once
#include "Modules/ModuleManager.h"

TAPREVIEW_API DECLARE_LOG_CATEGORY_EXTERN(LogTapReview, Log, All);

class FTapReviewModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    static inline FTapReviewModule& Get()
    {
        return FModuleManager::LoadModuleChecked<FTapReviewModule>("TapReview");
    }
    static inline bool IsAvailable()
    {
        return FModuleManager::Get().IsModuleLoaded("TapReview");
    }
};

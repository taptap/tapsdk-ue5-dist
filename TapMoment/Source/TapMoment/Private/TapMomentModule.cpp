// Copyright TapTap. All Rights Reserved.
#include "TapMomentModule.h"
#include "Modules/ModuleManager.h"
#include "Platform/ITapMomentPlatform.h"
#include "TapMomentCallbackManager.h"

DEFINE_LOG_CATEGORY(LogTapMoment);

void FTapMomentModule::StartupModule() {}
void FTapMomentModule::ShutdownModule()
{
    FTapMomentPlatformFactory::Reset();
    FTapMomentCallbackManager::Get().Reset();
}

IMPLEMENT_MODULE(FTapMomentModule, TapMoment)

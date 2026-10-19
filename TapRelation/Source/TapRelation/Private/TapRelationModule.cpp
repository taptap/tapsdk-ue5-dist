// Copyright TapTap. All Rights Reserved.
#include "TapRelationModule.h"
#include "Modules/ModuleManager.h"
#include "TapRelationCallbackManager.h"
#include "Platform/ITapRelationPlatform.h"
#if PLATFORM_IOS
#include "iOS/TapRelationIOSBridge.h"
#endif
DEFINE_LOG_CATEGORY(LogTapRelation);
void FTapRelationModule::StartupModule()
{
#if PLATFORM_IOS
    TapRelationIOSBridge::RegisterOpenURLHandler();
#endif
}
void FTapRelationModule::ShutdownModule()
{
#if PLATFORM_IOS
    TapRelationIOSBridge::UnregisterOpenURLHandler();
#endif
    FTapRelationPlatformFactory::Reset();
    FTapRelationCallbackManager::Get().Reset();
}
IMPLEMENT_MODULE(FTapRelationModule, TapRelation)

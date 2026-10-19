// Copyright TapTap. All Rights Reserved.
#include "TapRelationLiteModule.h"
#include "Modules/ModuleManager.h"
#include "TapRelationLiteCallbackManager.h"
#include "Platform/ITapRelationLitePlatform.h"
#if PLATFORM_IOS
#include "iOS/TapRelationLiteIOSBridge.h"
#endif
DEFINE_LOG_CATEGORY(LogTapRelationLite);
void FTapRelationLiteModule::StartupModule()
{
#if PLATFORM_IOS
    TapRelationLiteIOSBridge::RegisterOpenURLHandler();
#endif
}
void FTapRelationLiteModule::ShutdownModule()
{
#if PLATFORM_IOS
    TapRelationLiteIOSBridge::UnregisterOpenURLHandler();
    TapRelationLiteIOSBridge::Shutdown();
#endif
    FTapRelationLitePlatformFactory::Reset();
    FTapRelationLiteCallbackManager::Get().Reset();
}
IMPLEMENT_MODULE(FTapRelationLiteModule, TapRelationLite)

// Copyright TapTap. All Rights Reserved.
#include "TapLeaderboardModule.h"
#include "Modules/ModuleManager.h"
#include "Platform/ITapLeaderboardPlatform.h"
#include "TapLeaderboardCallbackManager.h"

DEFINE_LOG_CATEGORY(LogTapLeaderboard);

void FTapLeaderboardModule::StartupModule() {}
void FTapLeaderboardModule::ShutdownModule()
{
    FTapLeaderboardPlatformFactory::Reset();
    FTapLeaderboardCallbackManager::Get().Reset();
}

IMPLEMENT_MODULE(FTapLeaderboardModule, TapLeaderboard)

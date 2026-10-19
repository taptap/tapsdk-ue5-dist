// Copyright TapTap. All Rights Reserved.
#include "TapBattleModule.h"

#include "Platform/ITapBattlePlatform.h"
#include "TapOnlineBattleCallbackManager.h"

DEFINE_LOG_CATEGORY(LogTapBattle);

#define LOCTEXT_NAMESPACE "FTapBattleModule"

void FTapBattleModule::StartupModule()
{
	UE_LOG(LogTapBattle, Log, TEXT("TapBattle module started"));
}

void FTapBattleModule::ShutdownModule()
{
	// Drop the cached platform instance so its dtor frees any native handle
	// (mirrors FTapCloudSaveModule).
	FTapBattlePlatformFactory::Reset();
	FTapOnlineBattleCallbackManager::Get().Reset();
	UE_LOG(LogTapBattle, Log, TEXT("TapBattle module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTapBattleModule, TapBattle)

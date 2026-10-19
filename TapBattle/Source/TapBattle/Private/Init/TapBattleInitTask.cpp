// Copyright TapTap. All Rights Reserved.

#include "Init/TapBattleInitTask.h"
#include "TapBattleModule.h"
#include "TapBattle.h"
#include "Platform/ITapBattlePlatform.h"
#include "Internal/Init/TapInitTaskRegistry.h"

FTapTaskConfig FTapBattleInitTask::TaskConfig;
FTapBattleInitTask FTapBattleInitTask::StaticInstance;
FTapInitTaskAutoRegister<FTapBattleInitTask> FTapBattleInitTask::AutoRegister;

namespace
{
	struct FTaskConfigInitializer
	{
		FTaskConfigInitializer()
		{
			// Battle 业务通常依赖登录态，与 TapCloudSave 一致排在 TapLogin (20) 之后
			FTapBattleInitTask::WithPriority(30);
			FTapBattleInitTask::CanRunInBackground(false);
		}
	};
	static FTaskConfigInitializer GTaskConfigInit;
}

void FTapBattleInitTask::Execute(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapBattle, Log, TEXT("========================================"));
	UE_LOG(LogTapBattle, Log, TEXT("TapBattle Initialization Started"));
	UE_LOG(LogTapBattle, Log, TEXT("Version: %s"), *UTapBattle::GetVersion());
	UE_LOG(LogTapBattle, Log, TEXT("========================================"));

	if (auto P = FTapBattlePlatformFactory::CreatePlatform())
	{
		P->Initialize();
	}
	else
	{
		UE_LOG(LogTapBattle, Warning, TEXT("TapBattle platform not available; skipping init"));
	}

	UE_LOG(LogTapBattle, Log, TEXT("TapBattle Initialization Completed"));
}

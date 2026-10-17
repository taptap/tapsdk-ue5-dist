// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTapBattle, Log, All);

class FTapBattleModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FTapBattleModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FTapBattleModule>("TapBattle");
	}
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("TapBattle");
	}
};

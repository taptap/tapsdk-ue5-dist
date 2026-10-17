// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTapCloudSave, Log, All);

class FTapCloudSaveModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FTapCloudSaveModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FTapCloudSaveModule>("TapCloudSave");
	}
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("TapCloudSave");
	}
};

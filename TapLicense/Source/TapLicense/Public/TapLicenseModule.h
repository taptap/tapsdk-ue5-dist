// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTapLicense, Log, All);

class FTapLicenseModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FTapLicenseModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FTapLicenseModule>("TapLicense");
	}
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("TapLicense");
	}
};

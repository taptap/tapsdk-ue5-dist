// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// 声明 TapLogin 日志类别
DECLARE_LOG_CATEGORY_EXTERN(LogTapLogin, Log, All);

/**
 * TapLogin Module Interface
 */
class FTapLoginModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FTapLoginModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FTapLoginModule>("TapLogin");
	}

	/**
	 * Checks to see if this module is loaded and ready.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("TapLogin");
	}
};

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

// 声明 TapCore 日志类别
DECLARE_LOG_CATEGORY_EXTERN(LogTapCore, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTapBasic, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogTapDB, Log, All);

/**
 * TapSDK Module Interface
 */
class FTapCoreModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/**
	 * Singleton-like access to this module's interface.
	 * Beware of calling this during the shutdown phase, though. Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline FTapCoreModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FTapCoreModule>("TapCore");
	}

	/**
	 * Checks to see if this module is loaded and ready.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("TapCore");
	}
};

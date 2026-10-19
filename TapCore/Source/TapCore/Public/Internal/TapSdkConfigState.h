#pragma once

#include "TapTapSdkOptions.h"

/** Process-wide SDK state containing a copy of the current developer configuration and internal runtime settings. */
class TAPCORE_API FTapSdkConfigState
{
public:
	static void SetCurrentConfig(const FTapTapSdkOptions& Config);
	static const FTapTapSdkOptions& GetCurrentConfig();
	static void SetModuleOptions(FTapTapSdkOptions& Config, const TArray<FTapSdkOption>& ModuleOptions);
	static const TArray<FTapSdkOption>& GetModuleOptions(const FTapTapSdkOptions& Config);
	static void SetPreferredLanguage(ETapLanguage Language);
	/** Internal process-wide environment switch. Production is the default. */
	static void SetRNDEnvironment(bool bIsRND);
	static const FTapTapEventOptions& GetTapEventOptions(const FTapTapSdkOptions& Config);
	static bool IsRNDEnvironment();

	template <typename T>
	static const T* FindModuleOption(const FTapTapSdkOptions& Config)
	{
		for (int32 Index = Config.OtherOptions.Num() - 1; Index >= 0; --Index)
		{
			if (const T* Option = Config.OtherOptions[Index].template GetPtr<T>())
			{
				return Option;
			}
		}
		return nullptr;
	}

private:
	static FTapTapSdkOptions& GetMutableCurrentConfig();
	static bool& GetMutableRNDEnvironment();
};

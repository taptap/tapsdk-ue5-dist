// Copyright TapTap. All Rights Reserved.

#include "Internal/TapSdkConfigState.h"

FTapTapSdkOptions& FTapSdkConfigState::GetMutableCurrentConfig()
{
	static FTapTapSdkOptions CurrentConfig;
	return CurrentConfig;
}

bool& FTapSdkConfigState::GetMutableRNDEnvironment()
{
	static bool bRNDEnvironment = false;
	return bRNDEnvironment;
}

void FTapSdkConfigState::SetCurrentConfig(const FTapTapSdkOptions& InConfig)
{
	GetMutableCurrentConfig() = InConfig;
}

const FTapTapSdkOptions& FTapSdkConfigState::GetCurrentConfig()
{
	return GetMutableCurrentConfig();
}

void FTapSdkConfigState::SetModuleOptions(FTapTapSdkOptions& Config, const TArray<FTapSdkOption>& ModuleOptions)
{
	Config.OtherOptions = ModuleOptions;
}

const TArray<FTapSdkOption>& FTapSdkConfigState::GetModuleOptions(const FTapTapSdkOptions& Config)
{
	return Config.OtherOptions;
}

void FTapSdkConfigState::SetPreferredLanguage(ETapLanguage Language)
{
	GetMutableCurrentConfig().PreferredLanguage = Language;
}

void FTapSdkConfigState::SetRNDEnvironment(bool bIsRND)
{
	GetMutableRNDEnvironment() = bIsRND;
}

const FTapTapEventOptions& FTapSdkConfigState::GetTapEventOptions(const FTapTapSdkOptions& Config)
{
	if (const FTapTapEventOptions* Options = FindModuleOption<FTapTapEventOptions>(Config))
	{
		return *Options;
	}
	static const FTapTapEventOptions DefaultOptions;
	return DefaultOptions;
}

bool FTapSdkConfigState::IsRNDEnvironment()
{
	return GetMutableRNDEnvironment();
}

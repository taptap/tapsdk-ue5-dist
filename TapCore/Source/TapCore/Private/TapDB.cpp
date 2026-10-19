// Copyright TapTap. All Rights Reserved.

#include "Internal/TapDBInternal.h"
#include "Internal/TapSdkConfigState.h"
#include "TapCoreModule.h"
#include "TapTapEvent.h"

bool FTapDBInternal::bIsEnabled = false;

void FTapDBInternal::Init(const FTapTapSdkOptions& Config)
{
	const FTapTapEventOptions& EventOptions = FTapSdkConfigState::GetTapEventOptions(Config);
	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("TapDB Initialization Started"));
	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("EnableTapTapEvent: %s"), EventOptions.bEnableTapTapEvent ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTapCore, Log, TEXT("EnableLog: %s"), Config.bEnableLog ? TEXT("true") : TEXT("false"));

	bIsEnabled = EventOptions.bEnableTapTapEvent;
	if (!bIsEnabled)
	{
		UE_LOG(LogTapCore, Log, TEXT("TapTapEvent is disabled; skipping event initialization"));
		return;
	}

	FTapTapEvent::Init(Config.bEnableLog);

	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("TapDB Initialized Successfully"));
	UE_LOG(LogTapCore, Log, TEXT("========================================"));
}

bool FTapDBInternal::IsEnabled()
{
	return bIsEnabled;
}

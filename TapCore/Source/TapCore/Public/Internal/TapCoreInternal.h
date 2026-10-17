#pragma once

#include "TapCore.h"

/** Internal completion and endpoint access used by TapSDK modules. */
class TAPCORE_API FTapCoreInternal
{
public:
	static FString GetBaseUrl(bool bIsRND, ETapRegion Region);
	static FString GetGatekeeperConfigUrl(const FString& Key);
	static uint64 GetInitAttemptId();
	static bool IsInitAttemptActive(uint64 AttemptId);
	static void NotifyInitResult(int32 Code, const FString& Message, uint64 AttemptId);
	static void NotifyInitSuccess(uint64 AttemptId = 0, bool bClientInfoValidated = false);
	static void NotifyInitFailed(int32 ErrorCode, const FString& ErrorMessage, uint64 AttemptId = 0);
};

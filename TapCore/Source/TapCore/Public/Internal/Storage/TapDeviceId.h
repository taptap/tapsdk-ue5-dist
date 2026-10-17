// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Single source of truth for the SDK's device_id.
 *
 * Lazy-initialized on first Get(): reads persisted value, or generates one with
 * three-level fallback (GetDeviceId → GetMachineId → hashed system attributes),
 * then persists it under FTapMachineBoundPrefs (machine-bound).
 *
 * Both gatekeeper request headers and OpenLog upload in TapCore MUST
 * read through this — that is the only way the two ends stay in sync.
 */
class TAPCORE_API FTapDeviceId
{
public:
	static FString Get();

private:
	static FString CachedDeviceId;
	static bool bInitialized;
};

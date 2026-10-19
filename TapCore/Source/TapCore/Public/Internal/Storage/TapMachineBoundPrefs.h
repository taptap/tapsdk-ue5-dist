// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internal/Storage/TapPrefs.h"

/**
 * Machine-bound cache layer over FTapPrefs.
 *
 * On Init, compares the stored machine fingerprint against the current machine.
 * If they differ (cache file copied from another machine), all cached data is
 * physically cleared. Upper layers then regenerate values through their normal
 * "read miss → generate → save" logic with zero code changes.
 *
 * Fingerprint source (layered):
 *   Primary  — FPlatformMisc::GetMachineId()   (Win: Registry MachineGuid, Mac: Hardware UUID)
 *   Fallback — FPlatformMisc::GetDeviceId()
 */
class TAPCORE_API FTapMachineBoundPrefs
{
public:
	/** Initialize with a path relative to ProjectSavedDir. */
	void Init(const FString& RelativePath);

	FString GetString(const FString& Key, const FString& Default = TEXT("")) const;
	int64 GetInt64(const FString& Key, int64 Default = 0) const;
	void Set(const FString& Key, const FString& Value);
	void SetInt64(const FString& Key, int64 Value);
	bool Remove(const FString& Key);
	void Clear();
	void Save();

	/** True if Init() detected a machine mismatch and cleared the cache. */
	bool WasInvalidated() const { return bWasInvalidated; }

private:
	static FString ComputeFingerprint();

	FTapPrefs Prefs;
	FString Fingerprint;
	bool bWasInvalidated = false;
};

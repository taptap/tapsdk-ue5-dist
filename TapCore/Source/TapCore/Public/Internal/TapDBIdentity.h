// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FTapMachineBoundPrefs;

/**
 * Manages device identifiers:
 * - DeviceId: OS-level device unique identifier (matches Unity SystemInfo.deviceUniqueIdentifier)
 * - DeviceId5: per-launch unique ID = MD5(processCreationTime + PID), Windows only
 * - PersistentId: randomly generated UUID, persisted across sessions
 * - InstallationId: randomly generated UUID per installation, persisted
 */
class TAPCORE_API FTapDBIdentity
{
public:
	void Init(FTapMachineBoundPrefs& Prefs);

	FString GetDeviceId() const { return DeviceId; }
	FString GetDeviceId5() const { return DeviceId5; }
	FString GetPersistentId() const { return PersistentId; }
	FString GetInstallationId() const { return InstallationId; }

private:
	FString DeviceId;
	FString DeviceId5;
	FString PersistentId;
	FString InstallationId;
};

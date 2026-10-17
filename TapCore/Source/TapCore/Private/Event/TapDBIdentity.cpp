// Copyright TapTap. All Rights Reserved.

#include "Internal/TapDBIdentity.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/TapDBConstants.h"
#include "TapCoreModule.h"
#include "Internal/Storage/TapDeviceId.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	/** MD5 a pure-ASCII string -> 32-char lowercase hex. */
	FString Md5Hex(const FString& Input)
	{
		FMD5 Md5;
		FTCHARToUTF8 Utf8(*Input);
		Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
		uint8 Digest[16];
		Md5.Final(Digest);

		FString Hex;
		Hex.Reserve(32);
		for (int32 i = 0; i < 16; i++)
		{
			Hex += FString::Printf(TEXT("%02x"), Digest[i]);
		}
		return Hex;
	}

	/**
	 * Generate device_id5: per-launch unique ID = MD5(processCreationTime + "-" + PID)
	 * Matches Unity DeviceInfo.GetLaunchUniqueID() — Windows only.
	 */
	FString GenerateDeviceId5()
	{
#if PLATFORM_WINDOWS
		uint64 StartTime = 0;
		DWORD ProcessId = GetCurrentProcessId();

		FILETIME ftCreation, ftExit, ftKernel, ftUser;
		if (GetProcessTimes(GetCurrentProcess(), &ftCreation, &ftExit, &ftKernel, &ftUser))
		{
			StartTime = (static_cast<uint64>(ftCreation.dwHighDateTime) << 32)
				| ftCreation.dwLowDateTime;
		}

		return Md5Hex(FString::Printf(TEXT("%llu-%lu"), StartTime, ProcessId));
#else
		return TEXT("");
#endif
	}
}

void FTapDBIdentity::Init(FTapMachineBoundPrefs& Prefs)
{
	// Device ID — single source of truth shared with TapBasic (gatekeeper headers).
	// Persisted by FTapDeviceId, not by TapDB; the obsolete PREF_DEVICE_ID key is
	// no longer read or written.
	DeviceId = FTapDeviceId::Get();

	// Device ID5 — per-launch unique ID, Windows only (not persisted)
	DeviceId5 = GenerateDeviceId5();

	// Persistent ID
	PersistentId = Prefs.GetString(TapDBConstants::PREF_PERSISTENT_ID);
	if (PersistentId.IsEmpty())
	{
		PersistentId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		Prefs.Set(TapDBConstants::PREF_PERSISTENT_ID, PersistentId);
		Prefs.Save();
	}

	// Installation ID
	InstallationId = Prefs.GetString(TapDBConstants::PREF_INSTALLATION_ID);
	if (InstallationId.IsEmpty())
	{
		InstallationId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
		Prefs.Set(TapDBConstants::PREF_INSTALLATION_ID, InstallationId);
		Prefs.Save();
	}

	UE_LOG(LogTapDB, Log, TEXT("[TapDBIdentity] DeviceId=%s, DeviceId5=%s, PersistentId=%s, InstallationId=%s"),
		*DeviceId, *DeviceId5, *PersistentId, *InstallationId);
}

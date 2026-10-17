// Copyright TapTap. All Rights Reserved.

#include "Internal/Storage/TapDeviceId.h"

#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "TapCoreModule.h"
#include "Misc/Guid.h"
#include "Misc/SecureHash.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "HAL/PlatformProcess.h"

namespace
{
	const FString DEVICE_PREFS_PATH = TEXT("Tap/tap_device.json");
	const FString DEVICE_ID_KEY     = TEXT("device_id");

	FString GenerateDeviceIdFresh()
	{
		FString Id = FPlatformMisc::GetDeviceId();
		if (!Id.IsEmpty())
		{
			return Id;
		}

		const FGuid MachineGuid = FPlatformMisc::GetMachineId();
		if (MachineGuid.IsValid())
		{
			return MachineGuid.ToString(EGuidFormats::DigitsWithHyphensLower);
		}

		// Last-resort: hash stable system attributes so a machine reliably maps
		// to the same id even when both GetDeviceId/GetMachineId are empty.
		const FString Seed = FString::Printf(TEXT("%s|%s|%s"),
			*FPlatformMisc::GetLoginId(),
			FPlatformProcess::ComputerName(),
			*FPlatformMisc::GetOperatingSystemId());

		if (!Seed.IsEmpty() && Seed != TEXT("||"))
		{
			FMD5 Md5;
			FTCHARToUTF8 Utf8(*Seed);
			Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
			uint8 Digest[16];
			Md5.Final(Digest);
			return FGuid(
				*reinterpret_cast<uint32*>(&Digest[0]),
				*reinterpret_cast<uint32*>(&Digest[4]),
				*reinterpret_cast<uint32*>(&Digest[8]),
				*reinterpret_cast<uint32*>(&Digest[12])
			).ToString(EGuidFormats::DigitsWithHyphensLower);
		}

		return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}
}

FString FTapDeviceId::CachedDeviceId;
bool FTapDeviceId::bInitialized = false;

FString FTapDeviceId::Get()
{
	if (bInitialized)
	{
		return CachedDeviceId;
	}

	FTapMachineBoundPrefs Prefs;
	Prefs.Init(DEVICE_PREFS_PATH);

	CachedDeviceId = Prefs.GetString(DEVICE_ID_KEY);
	if (CachedDeviceId.IsEmpty())
	{
		CachedDeviceId = GenerateDeviceIdFresh();
		Prefs.Set(DEVICE_ID_KEY, CachedDeviceId);
		Prefs.Save();
		UE_LOG(LogTapBasic, Log, TEXT("[FTapDeviceId] generated & persisted: %s"), *CachedDeviceId);
	}
	else
	{
		UE_LOG(LogTapBasic, Log, TEXT("[FTapDeviceId] loaded from prefs: %s"), *CachedDeviceId);
	}

	bInitialized = true;
	return CachedDeviceId;
}

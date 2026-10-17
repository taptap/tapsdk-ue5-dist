// Copyright TapTap. All Rights Reserved.

#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "TapCoreModule.h"
#include "HAL/PlatformProcess.h"

namespace
{
	const FString FINGERPRINT_KEY = TEXT("tap_machine_fp");
}

void FTapMachineBoundPrefs::Init(const FString& RelativePath)
{
	Prefs.Init(RelativePath);
	Fingerprint = ComputeFingerprint();

	const FString StoredFP = Prefs.GetString(FINGERPRINT_KEY);

	// Only trust a match when both sides are non-empty
	if (!StoredFP.IsEmpty() && !Fingerprint.IsEmpty() && StoredFP == Fingerprint)
	{
		return; // Same machine — cache is valid
	}

	if (!StoredFP.IsEmpty())
	{
		UE_LOG(LogTapBasic, Warning,
			TEXT("[MachBoundPrefs] Machine fingerprint mismatch, cache cleared (stored=%s, current=%s)"),
			*StoredFP, *Fingerprint);
		Prefs.Clear();
		bWasInvalidated = true;
	}

	Prefs.Set(FINGERPRINT_KEY, Fingerprint);
	Prefs.Save();
}

FString FTapMachineBoundPrefs::GetString(const FString& Key, const FString& Default) const
{
	return Prefs.GetString(Key, Default);
}

int64 FTapMachineBoundPrefs::GetInt64(const FString& Key, int64 Default) const
{
	return Prefs.GetInt64(Key, Default);
}

void FTapMachineBoundPrefs::Set(const FString& Key, const FString& Value)
{
	Prefs.Set(Key, Value);
}

void FTapMachineBoundPrefs::SetInt64(const FString& Key, int64 Value)
{
	Prefs.SetInt64(Key, Value);
}

bool FTapMachineBoundPrefs::Remove(const FString& Key)
{
	return Prefs.Remove(Key);
}

void FTapMachineBoundPrefs::Clear()
{
	Prefs.Clear();
}

void FTapMachineBoundPrefs::Save()
{
	Prefs.Set(FINGERPRINT_KEY, Fingerprint);
	Prefs.Save();
}

FString FTapMachineBoundPrefs::ComputeFingerprint()
{
	FString Id = FPlatformMisc::GetMachineId().ToString();
	if (Id.IsEmpty() || Id == FGuid().ToString())
	{
		Id = FPlatformMisc::GetDeviceId();
	}
	if (Id.IsEmpty())
	{
		// Fallback: hash stable system attributes so empty never matches empty
		Id = FString::Printf(TEXT("%s_%s_%s"),
			*FPlatformMisc::GetLoginId(),
			FPlatformProcess::ComputerName(),
			*FPlatformMisc::GetOperatingSystemId());
	}
	return Id;
}

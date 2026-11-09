// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <windows.h>
#elif PLATFORM_MAC
#include <libproc.h>
#include <cerrno>
#endif

namespace TapProcessIdentity
{
// Unknown/denied access is never evidence that a process exited.
inline bool Query(uint32 Pid, FString& Started, bool& bExited)
{
	bExited = false;
#if PLATFORM_WINDOWS
	HANDLE Process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, Pid);
	if (!Process) { bExited = GetLastError() == ERROR_INVALID_PARAMETER; return bExited; }
	FILETIME Created{}, Exited{}, Kernel{}, User{};
	const bool bOk = GetProcessTimes(Process, &Created, &Exited, &Kernel, &User) != 0;
	CloseHandle(Process);
	if (!bOk) return false;
	bExited = Exited.dwHighDateTime != 0 || Exited.dwLowDateTime != 0;
	Started = FString::Printf(TEXT("%llu"), (static_cast<uint64>(Created.dwHighDateTime) << 32) | Created.dwLowDateTime);
	return true;
#elif PLATFORM_MAC
	proc_bsdinfo Info{};
	if (proc_pidinfo(Pid, PROC_PIDTBSDINFO, 0, &Info, sizeof(Info)) != sizeof(Info))
	{ bExited = errno == ESRCH; return bExited; }
	Started = FString::Printf(TEXT("%llu:%llu"), static_cast<uint64>(Info.pbi_start_tvsec), static_cast<uint64>(Info.pbi_start_tvusec));
	return true;
#else
	return false;
#endif
}

inline bool Register(const FString& RootHash, bool& bPrimary)
{
#if PLATFORM_WINDOWS
	// Every process retains a handle until OS termination, including secondary processes.
	// Closing SDK objects must not make a still-running process appear to have exited.
	static HANDLE Registration = nullptr;
	static FString RegisteredRoot;
	static bool bRegisteredPrimary = false;
	if (Registration)
	{
		bPrimary = bRegisteredPrimary;
		return RegisteredRoot == RootHash;
	}
	Registration = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, 1,
		*(TEXT("Local\\TapSDK.UE.Storage.") + RootHash));
	const DWORD Error = GetLastError();
	if (!Registration) return false;
	RegisteredRoot = RootHash;
	bPrimary = bRegisteredPrimary = Error != ERROR_ALREADY_EXISTS;
	return true;
#else
	return false;
#endif
}
}
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

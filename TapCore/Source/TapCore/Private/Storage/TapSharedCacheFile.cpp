// Copyright TapTap. All Rights Reserved.
#include "Internal/Storage/TapSharedCacheFile.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "TapSharedCacheFileNative.h"
namespace
{
	struct FDesktopCacheLease final : FTapSharedCacheFile::FLease
	{
		TapSharedCacheFileNative::FLease Lock;
	};
}
#endif
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

TUniquePtr<FTapSharedCacheFile::FLease> FTapSharedCacheFile::TryAcquire(const FString& Path)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	const FString FullPath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Path);
	if (!TapSharedCacheFileNative::MakeDirectories(TCHAR_TO_UTF8(*FPaths::GetPath(FullPath)))) return nullptr;
	auto Lease = MakeUnique<FDesktopCacheLease>();
	if (Lease->Lock.Acquire(TCHAR_TO_UTF8(*(FullPath + TEXT(".lock")))) != TapSharedCacheFileNative::EAcquire::Acquired)
		return nullptr;
	return Lease;
#else
	return MakeUnique<FLease>();
#endif
}

bool FTapSharedCacheFile::Write(const FString& Path, const FString& Contents)
{
	auto Lease = TryAcquire(Path);
	return Lease && WriteUnderLock(Path, Contents);
}

bool FTapSharedCacheFile::WriteUnderLock(const FString& Path, const FString& Contents)
{
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	return TapSharedCacheFileNative::Write(
		TCHAR_TO_UTF8(*IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Path)), TCHAR_TO_UTF8(*Contents));
#else
	return FFileHelper::SaveStringToFile(Contents, *Path);
#endif
}

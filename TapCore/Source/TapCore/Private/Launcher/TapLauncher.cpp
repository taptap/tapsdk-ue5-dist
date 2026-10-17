// Copyright TapTap. All Rights Reserved.
#include "Launcher/TapLauncher.h"
#include "Internal/TapLauncherInternal.h"

#if PLATFORM_WINDOWS
#include "Internal/Launcher/TapLauncherDesktopImpl.h"
#endif

FOnClientStateChanged UTapLauncher::OnClientStateChanged;
FOnLauncherCheckCompleted FTapLauncherInternal::OnLauncherCheckCompleted;

namespace
{
	void FireUnsupportedError(const FOnLauncherCheckError& OnError)
	{
		OnError.ExecuteIfBound(ETapLauncherInitResult::Unknown,
			TEXT("UTapLauncher: platform not supported (Win64 only)"));
	}

}

void UTapLauncher::IsLaunchedFromTapTapPC(
	FOnLauncherCheckSuccess OnSuccess,
	FOnLauncherCheckError   OnError)
{
#if PLATFORM_WINDOWS
	FTapLauncherDesktopImpl::Get().IsLaunchedFromTapTapPC(OnSuccess, OnError);
#else
	FireUnsupportedError(OnError);
#endif
}

bool UTapLauncher::IsPassedLauncherCheck()
{
#if PLATFORM_WINDOWS
	return FTapLauncherDesktopImpl::Get().IsPassedCheck();
#else
	return false;
#endif
}

bool FTapLauncherInternal::GetCachedOpenId(FString& OutOpenId)
{
#if PLATFORM_WINDOWS
	return FTapLauncherDesktopImpl::Get().GetCachedOpenId(OutOpenId);
#else
	OutOpenId.Empty();
	return false;
#endif
}



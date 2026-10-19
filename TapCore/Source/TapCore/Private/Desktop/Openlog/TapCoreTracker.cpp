// Copyright TapTap. All Rights Reserved.

#include "Platform/TapCoreTracker.h"
#include "Internal/Platform/TapOpenlog.h"

// TapCore_VERSION is defined by Build.cs from the .uplugin file.
// Provide a fallback just in case it's somehow missing.
#ifndef TapCore_VERSION
#define TapCore_VERSION TEXT("0.0.0")
#endif

FTapCoreTracker::FTapCoreTracker()
	: Tracker(TEXT("TapSDKCore"), TapCore_VERSION)
{
}

FTapCoreTracker& FTapCoreTracker::Get()
{
	static FTapCoreTracker Instance;
	return Instance;
}

void FTapCoreTracker::TrackInit()
{
	Tracker.TrackInit();
}

void FTapCoreTracker::TrackLaunchSuccess(const FString& LaunchSuccessType)
{
	TMap<FString, FString> Extra;
	if (!LaunchSuccessType.IsEmpty())
	{
		Extra.Add(TEXT("launch_success_type"), LaunchSuccessType);
	}
	Tracker.TrackSuccess(TEXT("launch"), TEXT(""), Extra);
}

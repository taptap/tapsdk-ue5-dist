// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internal/Platform/TapOpenlogTracker.h"

/**
 * Tracker for the TapCore module itself.
 *
 * Singleton; call FTapCoreTracker::Get().TrackInit() immediately after
 * FTapOpenlog::Init() during SDK initialisation.
 */
class TAPCORE_API FTapCoreTracker
{
public:
	static FTapCoreTracker& Get();

	/** Report that TapCore was successfully initialised. */
	void TrackInit();

	/**
	 * Report a successful launch.
	 * @param LaunchSuccessType  Optional type qualifier for the launch event.
	 */
	void TrackLaunchSuccess(const FString& LaunchSuccessType = TEXT(""));

private:
	FTapCoreTracker();

	FTapOpenlogTracker Tracker;
};

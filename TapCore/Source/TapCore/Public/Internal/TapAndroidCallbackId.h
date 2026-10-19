// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

/** Shared ID source for every module using TapCore's Android callback dispatcher. */
class TAPCORE_API FTapAndroidCallbackId
{
public:
	/** Returns a process-wide unique, nonzero ID. IDs are never recycled on unregister. */
	static int64 Allocate();
};

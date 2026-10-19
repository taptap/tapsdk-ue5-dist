// Copyright TapTap. All Rights Reserved.
#include "Internal/TapAndroidCallbackId.h"

#include <atomic>

int64 FTapAndroidCallbackId::Allocate()
{
	// Keep the counter in TapCore, not in a header or individual plugin module.
	static std::atomic<int64> NextId{1};
	return NextId.fetch_add(1, std::memory_order_relaxed);
}

#pragma once

#include "TapTapSdkOptions.h"

/** Internal TapDB lifecycle used by TapSDK Init and FTapTapEvent. Not a game facade. */
class TAPCORE_API FTapDBInternal
{
public:
	static void Init(const FTapTapSdkOptions& Config);
	static bool IsEnabled();

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTapEventInitializationTest;
#endif

	static bool bIsEnabled;
};

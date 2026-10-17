// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if !UE_BUILD_SHIPPING && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

/** Internal contract-test override. Construct and destroy on the game thread. */
class TAPCOMPLIANCE_API FTapComplianceTestOptionsScope
{
public:
	FTapComplianceTestOptionsScope(TOptional<bool> UseAgeRange, TOptional<bool> ShowSwitchAccount);
	~FTapComplianceTestOptionsScope();

	FTapComplianceTestOptionsScope(const FTapComplianceTestOptionsScope&) = delete;
	FTapComplianceTestOptionsScope& operator=(const FTapComplianceTestOptionsScope&) = delete;

private:
	bool bOriginalUseAgeRange;
	bool bOriginalShowSwitchAccount;
};

#endif

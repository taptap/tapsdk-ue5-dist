// Copyright TapTap. All Rights Reserved.

#include "Internal/TapComplianceTestOptionsScope.h"

#if !UE_BUILD_SHIPPING && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "Desktop/ComplianceState.h"
#include "Desktop/DesktopComplianceManager.h"

FTapComplianceTestOptionsScope::FTapComplianceTestOptionsScope(
	TOptional<bool> UseAgeRange, TOptional<bool> ShowSwitchAccount)
{
	check(IsInGameThread());
	const FComplianceState& State = FComplianceState::Get();
	bOriginalUseAgeRange = State.bUseAgeRange;
	bOriginalShowSwitchAccount = State.bShowSwitchAccount;
	FDesktopComplianceManager& Manager = FDesktopComplianceManager::Get();
	if (UseAgeRange.IsSet()) Manager.SetUseAgeRange(UseAgeRange.GetValue());
	if (ShowSwitchAccount.IsSet()) Manager.SetShowSwitchAccount(ShowSwitchAccount.GetValue());
}

FTapComplianceTestOptionsScope::~FTapComplianceTestOptionsScope()
{
	check(IsInGameThread());
	FDesktopComplianceManager& Manager = FDesktopComplianceManager::Get();
	Manager.SetUseAgeRange(bOriginalUseAgeRange);
	Manager.SetShowSwitchAccount(bOriginalShowSwitchAccount);
}

#endif

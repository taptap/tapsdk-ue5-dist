// Copyright TapTap. All Rights Reserved.
#include "Win64/TapLicensePlatformWin64.h"

#if PLATFORM_WINDOWS

#include "Win64/TapLicenseImpl.h"

void FTapLicensePlatformWin64::SetupCallbacks(
	FLicenseSuccessFn OnSuccess, FLicenseFailedFn OnFailed,
	FDLCQueryFn OnDLCQuery, FDLCOrderFn OnDLCOrder)
{
	FTapLicenseImpl::SetCallbacks(
		MoveTemp(OnSuccess), MoveTemp(OnFailed), MoveTemp(OnDLCQuery), MoveTemp(OnDLCOrder));
}

void FTapLicensePlatformWin64::Initialize()                              { FTapLicenseImpl::Initialize(); }
void FTapLicensePlatformWin64::Shutdown()                               { FTapLicenseImpl::Shutdown(); }
void FTapLicensePlatformWin64::CheckLicense(bool bForce)                { FTapLicenseImpl::CheckLicense(bForce); }
void FTapLicensePlatformWin64::QueryDLC(const TArray<FString>& DLCList) { FTapLicenseImpl::QueryDLC(DLCList); }
void FTapLicensePlatformWin64::PurchaseDLC(const FString& DLCID)        { FTapLicenseImpl::PurchaseDLC(DLCID); }

#endif // PLATFORM_WINDOWS

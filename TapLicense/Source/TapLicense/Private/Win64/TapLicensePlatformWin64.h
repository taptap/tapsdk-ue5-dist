// Copyright TapTap. All Rights Reserved.
#pragma once

#if PLATFORM_WINDOWS

#include "CoreMinimal.h"
#include "Platform/ITapLicensePlatform.h"

/** Win64 实现：转调现有 FTapLicenseImpl 静态状态机。 */
class FTapLicensePlatformWin64 : public ITapLicensePlatform
{
public:
	virtual void SetupCallbacks(FLicenseSuccessFn, FLicenseFailedFn, FDLCQueryFn, FDLCOrderFn) override;
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void CheckLicense(bool bForce) override;
	virtual void QueryDLC(const TArray<FString>& DLCList) override;
	virtual void PurchaseDLC(const FString& DLCID) override;
	virtual FString GetPlatformName() const override { return TEXT("Win64"); }
};

#endif // PLATFORM_WINDOWS

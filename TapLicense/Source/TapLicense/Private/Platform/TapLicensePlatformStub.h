// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Platform/ITapLicensePlatform.h"

#if PLATFORM_MAC || PLATFORM_LINUX

/** Mac/Linux stub：TapTap 正版验证产品上仅 Windows，这里全部 no-op。 */
class FTapLicensePlatformStub : public ITapLicensePlatform
{
public:
	virtual void SetupCallbacks(FLicenseSuccessFn, FLicenseFailedFn, FDLCQueryFn, FDLCOrderFn) override {}
	virtual void Initialize() override;
	virtual void Shutdown() override {}
	virtual void CheckLicense(bool /*bForce*/) override {}
	virtual void QueryDLC(const TArray<FString>& /*DLCList*/) override {}
	virtual void PurchaseDLC(const FString& /*DLCID*/) override {}
	virtual FString GetPlatformName() const override { return TEXT("Desktop-Stub"); }
};

#endif // PLATFORM_MAC || PLATFORM_LINUX

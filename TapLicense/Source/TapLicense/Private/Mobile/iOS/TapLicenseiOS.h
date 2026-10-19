// Copyright TapTap. All Rights Reserved.
#pragma once

#if PLATFORM_IOS

#include "CoreMinimal.h"
#include "Platform/ITapLicensePlatform.h"

/** iOS 实现：ObjC++ 桥接到原生 TapTapLicenseSDK。回调统一切回 GameThread。 */
class FTapLicenseiOS : public ITapLicensePlatform
{
public:
	virtual void SetupCallbacks(FLicenseSuccessFn, FLicenseFailedFn, FDLCQueryFn, FDLCOrderFn) override;
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void CheckLicense(bool bForce) override;
	virtual void QueryDLC(const TArray<FString>& DLCList) override;
	virtual void PurchaseDLC(const FString& DLCID) override;
	virtual FString GetPlatformName() const override { return TEXT("iOS"); }

private:
	FLicenseSuccessFn CbSuccess;
	FLicenseFailedFn  CbFailed;
	FDLCQueryFn       CbDLCQuery;
	FDLCOrderFn       CbDLCOrder;
};

#endif // PLATFORM_IOS

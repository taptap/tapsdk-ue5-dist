// Copyright TapTap. All Rights Reserved.
#include "Win64/TapLicenseAnalytics.h"

#include "Misc/Guid.h"

namespace TapLicenseAnalytics
{
	FString NewSessionId()
	{
		return FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	}

	FOperation MakeCheckLicenseOperation(bool bForce)
	{
		return {TEXT("checkLicense"), {{TEXT("license_type"), bForce ? TEXT("tap_license") : TEXT("local_license")}}};
	}

	FOperation MakeQueryDLCOperation(const TArray<FString>& DLCList)
	{
		return {TEXT("queryDLC"), {{TEXT("sku_ids"), FString::Join(DLCList, TEXT(","))}}};
	}

	FOperation MakePurchaseDLCOperation(const FString& DLCID)
	{
		return {TEXT("purchaseDLC"), {{TEXT("sku_id"), DLCID}}};
	}
}

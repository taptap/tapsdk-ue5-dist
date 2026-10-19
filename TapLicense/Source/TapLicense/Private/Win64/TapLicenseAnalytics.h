// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

namespace TapLicenseAnalytics
{
	struct FOperation
	{
		FString FuncName;
		TMap<FString, FString> Props;
	};

	FString NewSessionId();
	FOperation MakeCheckLicenseOperation(bool bForce = true);
	FOperation MakeQueryDLCOperation(const TArray<FString>& DLCList);
	FOperation MakePurchaseDLCOperation(const FString& DLCID);
}

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "TapComplianceOptions.generated.h"

/** TapCompliance initialization options, aligned with Unity TapTapComplianceOption. */
USTRUCT(BlueprintType)
struct TAPCOMPLIANCE_API FTapComplianceOptions : public FTapSdkBaseOptions
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCompliance")
	bool bShowSwitchAccount = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapCompliance")
	bool bUseAgeRange = true;
};

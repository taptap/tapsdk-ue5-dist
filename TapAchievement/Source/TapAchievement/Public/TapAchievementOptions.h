// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "TapAchievementOptions.generated.h"

/** TapAchievement initialization options, aligned with Unity TapTapAchievementOptions. */
USTRUCT(BlueprintType)
struct TAPACHIEVEMENT_API FTapAchievementOptions : public FTapSdkBaseOptions
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "TapAchievement")
	bool bEnableToast = true;
};

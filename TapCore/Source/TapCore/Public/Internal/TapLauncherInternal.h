#pragma once
#include "CoreMinimal.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLauncherCheckCompleted, const FString& /*OpenId*/);

/** Module-to-module login synchronization; not a game-facing API. */
class TAPCORE_API FTapLauncherInternal
{
public:
	static FOnLauncherCheckCompleted OnLauncherCheckCompleted;
	static bool GetCachedOpenId(FString& OutOpenId);
};

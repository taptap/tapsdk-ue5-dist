// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"

/** A detached view of the live UE receivers; weak references preserve their ownership. */
struct FTapLeaderboardCallbackState
{
	TArray<TWeakObjectPtr<UObject>> RegisteredCallbacks;
	TWeakObjectPtr<UObject> ShareCallback;
};

/** Internal diagnostics, not part of the Blueprint/game developer facade. */
class TAPLEADERBOARD_API FTapLeaderboardInternal
{
public:
	/** Read on the GameThread so both registries describe the same point in time. */
	static FTapLeaderboardCallbackState GetCallbackState();
};

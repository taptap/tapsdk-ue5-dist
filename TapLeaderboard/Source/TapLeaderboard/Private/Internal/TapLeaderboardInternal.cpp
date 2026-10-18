// Copyright TapTap. All Rights Reserved.

#include "Internal/TapLeaderboardInternal.h"
#include "TapLeaderboardCallbackManager.h"

FTapLeaderboardCallbackState FTapLeaderboardInternal::GetCallbackState()
{
	check(IsInGameThread());
	auto& Manager = FTapLeaderboardCallbackManager::Get();
	FTapLeaderboardCallbackState State;
	State.RegisteredCallbacks = Manager.Events.Snapshot();
	const auto ShareCallbacks = Manager.Share.Snapshot();
	if (!ShareCallbacks.IsEmpty()) State.ShareCallback = ShareCallbacks[0];
	return State;
}

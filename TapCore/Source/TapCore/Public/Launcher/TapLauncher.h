// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Launcher/TapLauncherTypes.h"
#include "TapLauncher.generated.h"

/**
 * PC Launcher verification API.
 * Win64 only. All other platforms: methods are no-ops that fire OnError.
 *
 * Usage:
 *   1. Call UTapLauncher::IsLaunchedFromTapTapPC(OnSuccess, OnError) after TapSDK Init.
 *   2. On success, License/DLC APIs become available.
 *   3. Subscribe to UTapLauncher::OnClientStateChanged for TapTap client status.
 */
UCLASS()
class TAPCORE_API UTapLauncher : public UObject
{
	GENERATED_BODY()
public:

	// ── Launcher verification ──────────────────────────────────────────
	// Equivalent to Unity TapClientStandalone.IsLaunchedFromTapTapPC()
	static void IsLaunchedFromTapTapPC(
		FOnLauncherCheckSuccess OnSuccess,
		FOnLauncherCheckError   OnError);

	// Returns cached result without triggering a DLL call.
	// Equivalent to Unity TapClientStandalone.isPassedInLaunchedFromTapTapPCCheck()
	static bool IsPassedLauncherCheck();

	// TapTap client running state (Online/Offline/Shutdown).
	// Subscribe before calling IsLaunchedFromTapTapPC to avoid missing the first event.
	// Equivalent to Unity TapClientStandalone.RegisterTapTapPCStateChangeListener
	static FOnClientStateChanged OnClientStateChanged;

};

// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Tickable.h"

#if PLATFORM_WINDOWS

/**
 * Drives TapSDK_RunCallbacks() every game-thread tick.
 * Equivalent to Unity's TapClientBridgePoll.Update().
 * Call Startup() after IsLaunchedFromTapTapPC succeeds.
 * Call Shutdown() in TapCoreModule::ShutdownModule().
 */
class FTapLauncherTickPump : public FTickableGameObject
{
public:
	static void Startup();
	static void Shutdown();

	// FTickableGameObject interface
	virtual void    Tick(float DeltaTime) override;
	virtual bool    IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual bool    IsTickableInEditor() const override { return false; }

private:
	static TUniquePtr<FTapLauncherTickPump> Instance;
};

#endif // PLATFORM_WINDOWS

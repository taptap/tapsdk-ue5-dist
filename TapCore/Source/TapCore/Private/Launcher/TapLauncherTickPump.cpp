// Copyright TapTap. All Rights Reserved.
#include "Launcher/TapLauncherTickPump.h"

#if PLATFORM_WINDOWS

#include "Internal/Launcher/TapLauncherDllWrapper.h"
#include "Stats/Stats.h"

TUniquePtr<FTapLauncherTickPump> FTapLauncherTickPump::Instance;

void FTapLauncherTickPump::Startup()
{
	if (!Instance.IsValid())
	{
		Instance = MakeUnique<FTapLauncherTickPump>();
	}
}

void FTapLauncherTickPump::Shutdown()
{
	Instance.Reset();
}

void FTapLauncherTickPump::Tick(float /*DeltaTime*/)
{
	TapLauncherDllWrapper::RunCallbacks();
}

bool FTapLauncherTickPump::IsTickable() const
{
	return Instance.IsValid();
}

TStatId FTapLauncherTickPump::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FTapLauncherTickPump, STATGROUP_Tickables);
}

#endif // PLATFORM_WINDOWS

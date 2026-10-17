// Copyright TapTap. All Rights Reserved.

#include "TapCoreModule.h"
#include "Internal/TapDBEventTracker.h"
#include "UObject/CoreRedirects.h"

#if PLATFORM_WINDOWS
#include "Internal/Launcher/TapLauncherDesktopImpl.h"
#endif

// 定义 TapCore 日志类别
DEFINE_LOG_CATEGORY(LogTapCore);
DEFINE_LOG_CATEGORY(LogTapBasic);
DEFINE_LOG_CATEGORY(LogTapDB);

#define LOCTEXT_NAMESPACE "FTapCoreModule"

void FTapCoreModule::StartupModule()
{
	// Preserve serialized Blueprint references to the renamed initialization options.
	TArray<FCoreRedirect> Redirects;
	Redirects.Emplace(ECoreRedirectFlags::Type_Struct,
		TEXT("/Script/TapCore.TapConfig"), TEXT("/Script/TapCore.TapTapSdkOptions"));
	// All TapBasic reflected types keep their names. Redirect the script package
	// so both serialized imports and LoadObject resolve to the merged module.
	Redirects.Emplace(ECoreRedirectFlags::Type_Package,
		TEXT("/Script/TapBasic"), TEXT("/Script/TapCore"));
	FCoreRedirects::AddRedirectList(Redirects, TEXT("TapCore"));

	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE_LOG(LogTapCore, Log, TEXT("TapSDK module started"));
}

void FTapCoreModule::ShutdownModule()
{
	// Stop event timers/callbacks and persist pending events while UE is still alive.
	// Do not leave this to the singleton destructor during process teardown.
	FTapDBEventTracker::Get().Shutdown();
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	UE_LOG(LogTapCore, Log, TEXT("TapSDK module shutdown"));
#if PLATFORM_WINDOWS
	FTapLauncherDesktopImpl::Get().Shutdown();
#endif
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTapCoreModule, TapCore)

// Copyright TapTap. All Rights Reserved.

#include "Desktop/Openlog/TapOpenlogNativeAPI.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapOpenlogNative, Log, All);

// ── Static member definitions ─────────────────────────────────────────────────

bool                           FTapOpenlogNativeAPI::bIsLoaded                  = false;
void*                          FTapOpenlogNativeAPI::DllHandle                  = nullptr;
FTdkOnAppStartedFn             FTapOpenlogNativeAPI::PfnOnAppStarted            = nullptr;
FTdkOnAppStoppedFn             FTapOpenlogNativeAPI::PfnOnAppStopped            = nullptr;
FTdkEnableModulesFn            FTapOpenlogNativeAPI::PfnEnableModules           = nullptr;
FTdkDisableModulesFn           FTapOpenlogNativeAPI::PfnDisableModules          = nullptr;
FTdkOpenLogFn                  FTapOpenlogNativeAPI::PfnOpenLog                 = nullptr;
FTdkOnLoginFn                  FTapOpenlogNativeAPI::PfnOnLogin                 = nullptr;
FTdkOnLogoutFn                 FTapOpenlogNativeAPI::PfnOnLogout                = nullptr;
FTdkOnForegroundFn             FTapOpenlogNativeAPI::PfnOnForeground            = nullptr;
FTdkOnBackgroundFn             FTapOpenlogNativeAPI::PfnOnBackground            = nullptr;
FTdkSetExtraAppDurationParamsFn FTapOpenlogNativeAPI::PfnSetExtraAppDurationParams = nullptr;
FTdkSetLogLevelFn              FTapOpenlogNativeAPI::PfnSetLogLevel             = nullptr;
FTdkVersionFn                  FTapOpenlogNativeAPI::PfnVersion                 = nullptr;
FTdkGitCommitFn                FTapOpenlogNativeAPI::PfnGitCommit               = nullptr;

// ── Helper macro ──────────────────────────────────────────────────────────────

#define RESOLVE_SYMBOL(Handle, FnPtr, Name) \
	FnPtr = reinterpret_cast<decltype(FnPtr)>(FPlatformProcess::GetDllExport(Handle, TEXT(Name))); \
	if (!FnPtr) { UE_LOG(LogTapOpenlogNative, Warning, TEXT("Failed to resolve symbol: " Name)); bAllResolved = false; }

// ── Load / Unload ─────────────────────────────────────────────────────────────

bool FTapOpenlogNativeAPI::Load()
{
	if (bIsLoaded)
	{
		return true;
	}

#if PLATFORM_WINDOWS
	const FString LibName = TEXT("tapsdkcore");
#elif PLATFORM_MAC
	const FString LibName = TEXT("libtapsdkcorecpp.dylib");
#else
	const FString LibName = TEXT("libtapsdkcorecpp.so");
#endif

	// Try to load from the binary output directory first (packaged builds),
	// then from the source ThirdParty directory (editor / local build).
	FString LibPath = FPaths::Combine(FPlatformProcess::BaseDir(), LibName);

	DllHandle = FPlatformProcess::GetDllHandle(*LibPath);

	if (!DllHandle)
	{
		// Fallback: ThirdParty source directory (for editor builds)
#if PLATFORM_WINDOWS
		LibPath = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("TapCore/Source/ThirdParty/Desktop/Win64/tapsdkcore.dll"));
#elif PLATFORM_MAC
		LibPath = FPaths::Combine(
			FPaths::ProjectPluginsDir(),
			TEXT("TapCore/Source/ThirdParty/Desktop/Mac/libtapsdkcorecpp.dylib"));
#endif
		DllHandle = FPlatformProcess::GetDllHandle(*LibPath);
	}

	if (!DllHandle)
	{
		UE_LOG(LogTapOpenlogNative, Warning,
			TEXT("Failed to load tapsdkcore native library from '%s'. OpenLog will be disabled."), *LibPath);
		return false;
	}

	UE_LOG(LogTapOpenlogNative, Log, TEXT("Loaded tapsdkcore native library from '%s'"), *LibPath);

	bool bAllResolved = true;
	RESOLVE_SYMBOL(DllHandle, PfnOnAppStarted,             "TdkOnAppStarted");
	RESOLVE_SYMBOL(DllHandle, PfnOnAppStopped,             "TdkOnAppStopped");
	RESOLVE_SYMBOL(DllHandle, PfnEnableModules,            "TdkEnableModules");
	RESOLVE_SYMBOL(DllHandle, PfnDisableModules,           "TdkDisableModules");
	RESOLVE_SYMBOL(DllHandle, PfnOpenLog,                  "TdkOpenLog");
	RESOLVE_SYMBOL(DllHandle, PfnOnLogin,                  "TdkOnLogin");
	RESOLVE_SYMBOL(DllHandle, PfnOnLogout,                 "TdkOnLogout");
	RESOLVE_SYMBOL(DllHandle, PfnOnForeground,             "TdkOnForeground");
	RESOLVE_SYMBOL(DllHandle, PfnOnBackground,             "TdkOnBackground");
	RESOLVE_SYMBOL(DllHandle, PfnSetExtraAppDurationParams,"TdkSetExtraAppDurationParams");
	RESOLVE_SYMBOL(DllHandle, PfnSetLogLevel,              "TdkSetLogLevel");
	RESOLVE_SYMBOL(DllHandle, PfnVersion,                  "TdkVersion");
	RESOLVE_SYMBOL(DllHandle, PfnGitCommit,                "TdkGitCommit");

	if (!bAllResolved)
	{
		UE_LOG(LogTapOpenlogNative, Warning, TEXT("Some tapsdkcore symbols could not be resolved; OpenLog may not work correctly."));
	}

	bIsLoaded = true;
	return true;
}

void FTapOpenlogNativeAPI::Unload()
{
	if (DllHandle)
	{
		FPlatformProcess::FreeDllHandle(DllHandle);
		DllHandle = nullptr;
	}
	bIsLoaded = false;

	PfnOnAppStarted             = nullptr;
	PfnOnAppStopped             = nullptr;
	PfnEnableModules            = nullptr;
	PfnDisableModules           = nullptr;
	PfnOpenLog                  = nullptr;
	PfnOnLogin                  = nullptr;
	PfnOnLogout                 = nullptr;
	PfnOnForeground             = nullptr;
	PfnOnBackground             = nullptr;
	PfnSetExtraAppDurationParams = nullptr;
	PfnSetLogLevel              = nullptr;
	PfnVersion                  = nullptr;
	PfnGitCommit                = nullptr;
}

// ── Wrapped API ───────────────────────────────────────────────────────────────

#define CHECK_LOADED(RetVal) \
	if (!bIsLoaded || !DllHandle) { UE_LOG(LogTapOpenlogNative, Warning, TEXT("tapsdkcore not loaded")); return RetVal; }

int FTapOpenlogNativeAPI::OnAppStarted(const char* Cfg, FTdkCommonVariablesGetter Getter, FTdkMemoryDeallocator Dealloc)
{
	CHECK_LOADED(-1);
	if (!PfnOnAppStarted) { return -1; }
	return PfnOnAppStarted(Cfg, Getter, Dealloc);
}

void FTapOpenlogNativeAPI::OnAppStopped()
{
	CHECK_LOADED(/*void*/);
	if (PfnOnAppStopped) PfnOnAppStopped();
}

void FTapOpenlogNativeAPI::EnableModules(const char* Modules)
{
	CHECK_LOADED(/*void*/);
	if (PfnEnableModules) PfnEnableModules(Modules);
}

void FTapOpenlogNativeAPI::DisableModules(const char* Modules)
{
	CHECK_LOADED(/*void*/);
	if (PfnDisableModules) PfnDisableModules(Modules);
}

void FTapOpenlogNativeAPI::OpenLog(const char* LogStore, const char* LogContent)
{
	CHECK_LOADED(/*void*/);
	if (PfnOpenLog) PfnOpenLog(LogStore, LogContent);
}

void FTapOpenlogNativeAPI::OnLogin(const char* UserInfo)
{
	CHECK_LOADED(/*void*/);
	if (PfnOnLogin) PfnOnLogin(UserInfo);
}

void FTapOpenlogNativeAPI::OnLogout()
{
	CHECK_LOADED(/*void*/);
	if (PfnOnLogout) PfnOnLogout();
}

void FTapOpenlogNativeAPI::OnForeground()
{
	CHECK_LOADED(/*void*/);
	if (PfnOnForeground) PfnOnForeground();
}

void FTapOpenlogNativeAPI::OnBackground()
{
	CHECK_LOADED(/*void*/);
	if (PfnOnBackground) PfnOnBackground();
}

void FTapOpenlogNativeAPI::SetExtraAppDurationParams(const char* Params)
{
	CHECK_LOADED(/*void*/);
	if (PfnSetExtraAppDurationParams) PfnSetExtraAppDurationParams(Params);
}

void FTapOpenlogNativeAPI::SetLogLevel(int InLogLevel, int InLogToConsole, int InEnableAutoTestLog)
{
	CHECK_LOADED(/*void*/);
	if (PfnSetLogLevel) PfnSetLogLevel(InLogLevel, InLogToConsole, InEnableAutoTestLog);
}

FString FTapOpenlogNativeAPI::GetVersion()
{
	if (!bIsLoaded || !PfnVersion) { return TEXT(""); }
	const char* Ver = PfnVersion();
	return Ver ? FString(UTF8_TO_TCHAR(Ver)) : TEXT("");
}

FString FTapOpenlogNativeAPI::GetGitCommit()
{
	if (!bIsLoaded || !PfnGitCommit) { return TEXT(""); }
	const char* Commit = PfnGitCommit();
	return Commit ? FString(UTF8_TO_TCHAR(Commit)) : TEXT("");
}

#undef CHECK_LOADED
#undef RESOLVE_SYMBOL

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// C API function pointer types (matching tapsdk_api.h)
typedef const char* (*FTdkCommonVariablesGetter)();
typedef void (*FTdkMemoryDeallocator)(const char*);

typedef int  (*FTdkOnAppStartedFn)(const char* cfg, FTdkCommonVariablesGetter getter, FTdkMemoryDeallocator dealloc);
typedef void (*FTdkOnAppStoppedFn)();
typedef void (*FTdkEnableModulesFn)(const char* modules);
typedef void (*FTdkDisableModulesFn)(const char* modules);
typedef void (*FTdkOpenLogFn)(const char* logStore, const char* logContent);
typedef void (*FTdkOnLoginFn)(const char* userInfo);
typedef void (*FTdkOnLogoutFn)();
typedef void (*FTdkOnForegroundFn)();
typedef void (*FTdkOnBackgroundFn)();
typedef void (*FTdkSetExtraAppDurationParamsFn)(const char* params);
typedef void (*FTdkSetLogLevelFn)(int logLevel, int logToConsole, int enableAutoTestLog);
typedef const char* (*FTdkVersionFn)();
typedef const char* (*FTdkGitCommitFn)();

/**
 * Wrapper for the platform-native tapsdkcore dynamic library.
 * Loads the library lazily and resolves all C API function pointers.
 * All methods are safe to call even if the library failed to load —
 * they will log a warning and return a sensible default.
 */
class FTapOpenlogNativeAPI
{
public:
	/** Load the native library and resolve function pointers. Returns true on success. */
	static bool Load();

	/** Unload the native library (called on app exit). */
	static void Unload();

	/** Returns true if the library was loaded successfully. */
	static bool IsLoaded() { return bIsLoaded; }

	// ── Wrapped API ───────────────────────────────────────────────────────────

	/** Initialize the native SDK. Returns 0 on success, -1 on failure. */
	static int OnAppStarted(const char* Cfg, FTdkCommonVariablesGetter Getter, FTdkMemoryDeallocator Dealloc);

	/** Notify the native SDK that the app is exiting. */
	static void OnAppStopped();

	/** Enable one or more functional modules (JSON array string, e.g. ["app_duration"]). */
	static void EnableModules(const char* Modules);

	/** Disable one or more functional modules. */
	static void DisableModules(const char* Modules);

	/** Upload a log entry. logStore is "tapsdk" or "tapsdk-apm". */
	static void OpenLog(const char* LogStore, const char* LogContent);

	/** Notify the native SDK that the user has logged in. */
	static void OnLogin(const char* UserInfo);

	/** Notify the native SDK that the user has logged out. */
	static void OnLogout();

	/** Notify the native SDK that the app has moved to the foreground. */
	static void OnForeground();

	/** Notify the native SDK that the app has moved to the background. */
	static void OnBackground();

	/** Set extra parameters for the app-duration module. */
	static void SetExtraAppDurationParams(const char* Params);

	/** Set the SDK log level. */
	static void SetLogLevel(int InLogLevel, int InLogToConsole, int InEnableAutoTestLog);

	/** Get the native library version string (e.g. "1.2.5"). May return empty string. */
	static FString GetVersion();

	/** Get the native library git-commit string. May return empty string. */
	static FString GetGitCommit();

private:
	static bool bIsLoaded;
	static void* DllHandle;

	// Function pointers
	static FTdkOnAppStartedFn             PfnOnAppStarted;
	static FTdkOnAppStoppedFn             PfnOnAppStopped;
	static FTdkEnableModulesFn            PfnEnableModules;
	static FTdkDisableModulesFn           PfnDisableModules;
	static FTdkOpenLogFn                  PfnOpenLog;
	static FTdkOnLoginFn                  PfnOnLogin;
	static FTdkOnLogoutFn                 PfnOnLogout;
	static FTdkOnForegroundFn             PfnOnForeground;
	static FTdkOnBackgroundFn             PfnOnBackground;
	static FTdkSetExtraAppDurationParamsFn PfnSetExtraAppDurationParams;
	static FTdkSetLogLevelFn              PfnSetLogLevel;
	static FTdkVersionFn                  PfnVersion;
	static FTdkGitCommitFn               PfnGitCommit;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

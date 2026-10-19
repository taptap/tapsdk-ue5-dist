// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS

/**
 * Thin wrapper around taptap_api.dll.
 * Loads the DLL on first call to Load(); all functions are no-ops if not loaded.
 * Mirror of Unity TapClientBridge + TapLicenseClientBridge.
 */
namespace TapLauncherDllWrapper
{
	/** Load taptap_api.dll. Returns true on success. */
	TAPCORE_API bool Load();

	/** Unload the DLL and clear all function pointers. */
	TAPCORE_API void Unload();

	/** Returns true if DLL is loaded and all required symbols resolved. */
	TAPCORE_API bool IsLoaded();

	/** Returns whether Unity's Tap PC authorization export is available. */
	TAPCORE_API bool HasAsyncAuthorizeInternal();

	// ── Launcher verification ──────────────────────────────────────────
	TAPCORE_API bool  RestartAppIfNecessary(const FString& ClientId);
	TAPCORE_API int32 Init(FString& OutErrMsg, const FString& PubKey);
	TAPCORE_API void  Shutdown();
	TAPCORE_API bool  GetClientId(FString& OutClientId);
	TAPCORE_API bool  GetOpenId(FString& OutOpenId);
	TAPCORE_API void  RunCallbacks();

	using FNativeCallbackFn = void(*)(int32 EventId, void* UserData);
	TAPCORE_API void  RegisterCallback(int32 EventId, FNativeCallbackFn Fn);
	TAPCORE_API void  UnregisterCallback(int32 EventId, FNativeCallbackFn Fn);

	/** Returns whether the native callback registration exports are available. */
	TAPCORE_API bool HasCallbackExports();

	// ── Unity TapLoginClientBridge authorization ─────────────────────────
	TAPCORE_API int32 AsyncAuthorizeInternal(
		const FString& ScopeStrings,
		const FString& ResponseType,
		const FString& RedirectUri,
		const FString& CodeChallenge,
		const FString& State,
		const FString& CodeChallengeMethod,
		const FString& VersionCode,
		const FString& SdkUa,
		const FString& Info);

	// ── License / DLC ─────────────────────────────────────────────────
	TAPCORE_API bool  HasLicense();
	TAPCORE_API bool  IsDLCOwned(const FString& SkuId);
	TAPCORE_API bool  ShowDLCStore(const FString& SkuId);
}

#endif // PLATFORM_WINDOWS

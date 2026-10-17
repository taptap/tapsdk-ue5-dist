// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Main OpenLog controller for Desktop (Win64/Mac/Linux).
 *
 * Loads the tapsdkcore native library, collects device parameters,
 * initialises the C++ SDK and exposes helpers used by per-module Trackers.
 *
 * Call FTapOpenlog::Init() once after the SDK configuration is stored.
 * Device identity comes from TapCore storage and does not require event Tracker initialization.
 */
class TAPCORE_API FTapOpenlog
{
public:
	/** Initialise OpenLog using the stored SDK configuration. */
	static void Init();

	/** Returns true if the native library was loaded and initialised successfully. */
	static bool IsInitialized();

	// ── Dynamic state setters (thread-safe) ──────────────────────────────────

	/** Set/update the TapTap OpenId reported in dynamic common variables. */
	static void SetOpenId(const FString& OpenId);

	/** Set/update the game user ID reported in dynamic common variables. */
	static void SetGameUserId(const FString& UserId);

	// ── Log upload helpers ────────────────────────────────────────────────────

	/**
	 * Upload a business log.
	 *   Project  – SDK project name, e.g. "TapSDKCore"
	 *   Version  – SDK project version
	 *   Action   – event action string
	 *   Props    – additional key/value pairs (may be empty)
	 */
	static void LogBusiness(const FString& Project, const FString& Version,
	                        const FString& Action, const TMap<FString, FString>& Props = {});

	/**
	 * Upload a technology (APM) log.
	 * Skipped for CN region unless Project == "TapPayment".
	 */
	static void LogTechnology(const FString& Project, const FString& Version,
	                          const FString& Action, const TMap<FString, FString>& Props = {});

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTapEventInitializationTest;
	friend class FTapOpenlogGameVersionTest;
#endif

	static bool bInitialized;

	// Dynamic common variables (protected by a critical section because
	// the native library may call the getter from a background thread).
	static FCriticalSection DynamicVarsMutex;
	static FString          CurrentOpenId;
	static FString          CurrentGameUserId;
	static FString          CurrentSdkLocale;

	// One-time parameters collected at Init time
	static FString CachedCommonJson;   // "common" object JSON
	static FString CachedStartJson;    // full start-config JSON

	/** Collect one-time device / app parameters into CachedCommonJson. */
	static void CollectGeneralParameters(TSharedRef<class FJsonObject>& OutObj);

	/** Build the full start-config JSON string. */
	static FString BuildStartConfig(const TSharedRef<class FJsonObject>& CommonObj);

	/** Bind UE lifecycle delegates (foreground/background/exit). */
	static void BindLifecycle();

	/** Map ETapLanguage to a locale string (e.g. "zh_CN"). */
	static FString LanguageToLocale();

	// ── C callbacks passed to TdkOnAppStarted ────────────────────────────────

	/** Called by the native library whenever it needs the dynamic common variables. */
	static const char* NativeGetCommonVariables();

	/** Called by the native library to free memory allocated by NativeGetCommonVariables. */
	static void NativeFreeString(const char* Ptr);
};

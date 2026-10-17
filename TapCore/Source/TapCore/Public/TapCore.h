// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
class ITapCorePlatform;

/** Initialization callback contract aligned with Unity TapTapSDK. */
class TAPCORE_API ITapInitCallback
{
public:
	virtual ~ITapInitCallback() = default;
	virtual void OnInitSuccess() = 0;
	virtual void OnInitFail(int32 ErrorCode, const FString& ErrorMessage) = 0;
};

/**
 * TapSDK Main Interface
 * Provides unified initialization entry point across all platforms
 *
 * Platform Detection:
 * - Android: Uses JNI to bridge to Android Native SDK
 * - iOS: Uses Objective-C++ to bridge to iOS Framework
 * - Desktop: Uses HTTP API or desktop-specific implementation
 */
class TAPCORE_API FTapTapSDK
{
public:
	/**
	 * Initialize TapSDK
	 * This will execute all registered initialization tasks in dependency order
	 * Platform-specific implementation will be automatically selected at compile time
	 *
	 * @param Config TapSDK configuration
	 */
	static void Init(const FTapTapSdkOptions& Config);

	/** Initialize TapSDK with module-specific options. */
	static void Init(const FTapTapSdkOptions& Config, const TArray<FTapSdkOption>& OtherOptions);

	/**
	 * Get TapSDK version
	 * @return Version string (includes platform info)
	 */
	static FString GetVersion();

	/**
	 * Check if TapSDK is initialized
	 * @return true if initialized
	 */
	static bool IsInitialized();

	/**
	 * Update language setting
	 * Change the display language for TapSDK UI components
	 * @param Language Target language to switch to
	 */
	static void UpdateLanguage(ETapLanguage Language);

	/** Register an initialization callback. Safe to call before or after Init(). */
	static void AddInitCallback(ITapInitCallback* Callback);

	/** Remove a previously registered initialization callback. */
	static void RemoveInitCallback(ITapInitCallback* Callback);

private:
	friend class FTapCoreInternal;
	friend class FTapCoreInitRetryTest;
	static bool BeginInitialization();
	static uint64 InitAttemptId;
	static void InitConfigured(const FTapTapSdkOptions& Config);
	/** Initialized flag */
	static bool bIsInitialized;
	static bool bInitInProgress;
	static int32 InitErrorCode;
	static FString InitErrorMessage;
	static TArray<ITapInitCallback*> InitCallbacks;
	static FCriticalSection InitCallbackLock;


	/** Platform-specific implementation instance */
	static TUniquePtr<ITapCorePlatform> PlatformImpl;

	/** Create platform-specific implementation based on compile-time platform */
	static void CreatePlatformImpl();
};

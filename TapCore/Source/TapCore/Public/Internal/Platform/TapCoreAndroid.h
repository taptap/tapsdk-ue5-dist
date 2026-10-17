// Copyright TapTap. All Rights Reserved.

#pragma once

// Only compile on Android platform
#if PLATFORM_ANDROID

#include "CoreMinimal.h"
#include "Internal/Platform/ITapCorePlatform.h"
#include <jni.h>

/**
 * Android Platform Implementation for TapSDK Core
 * Uses JNI to bridge to Android Native SDK via Router pattern
 */
class TAPCORE_API FTapCoreAndroid : public ITapCorePlatform
{
public:
	FTapCoreAndroid();
	virtual ~FTapCoreAndroid();

	// ITapCorePlatform interface
	virtual void Init(const FTapTapSdkOptions& Config) override;
	virtual FString GetPlatformVersion() const override;
	virtual bool IsInitialized() const override;
	virtual void OnInitResult(bool bSuccess) override { bIsInitialized = bSuccess; }
	virtual FString GetPlatformName() const override;
	virtual void UpdateLanguage(ETapLanguage Language) override;
	virtual void RequestGatekeeperValidation(const FTapTapSdkOptions& Config) override;

	// Router methods - Internal infrastructure for Android SDK bridge
	// These methods are kept as static utilities for internal use and potential future modules (e.g., TapDB)
	// They are NOT exposed in the public FTapTapSDK API
public:
	/**
	 * Call TapCore method synchronously through Router
	 *
	 * INTERNAL USE ONLY - Use FTapTapSDK public API instead
	 *
	 * @param MethodName Router method name (use TapCore::RouterMethods constants)
	 * @param ParamsJson Optional parameters JSON string
	 * @return Result JSON string: {"success": true/false, "data": "...", "message": "..."}
	 */
	static FString CallModuleRoute(const FString& MethodName, const FString& ParamsJson = TEXT(""));

	/**
	 * Call TapCore method asynchronously through Router
	 *
	 * INTERNAL USE ONLY - Reserved for future async operations
	 *
	 * @param MethodName Router method name (use TapCore::RouterMethods constants)
	 * @param ParamsJson Parameters JSON string
	 * @param Callback Callback function called when async operation completes
	 */
	static void CallModuleRouteAsync(
		const FString& MethodName,
		const FString& ParamsJson,
		TFunction<void(const FString&)> Callback);

	// JNI Callback Management
public:
	/**
	 * Create EngineBridgeCallback JNI object
	 *
	 * @param Env JNI environment
	 * @param Callback C++ callback function
	 * @return Global reference to EngineBridgeCallbackProxy object
	 */
	static jobject CreateEngineBridgeCallback(JNIEnv* Env, TFunction<void(const FString&)> Callback);

	/**
	 * Destroy EngineBridgeCallback JNI object
	 *
	 * @param Env JNI environment
	 * @param CallbackObj Callback object to destroy
	 */
	static void DestroyEngineBridgeCallback(JNIEnv* Env, jobject CallbackObj);

	/**
	 * Register callback function
	 *
	 * @param CallbackId ID allocated by FTapAndroidCallbackId::Allocate()
	 * @param Callback C++ callback function
	 */
	static void RegisterCallback(int64 CallbackId, TFunction<void(const FString&)> Callback);

	/**
	 * Trigger and remove callback
	 *
	 * @param CallbackId Callback ID
	 * @param ResultJson Result JSON string
	 */
	static void TriggerCallback(int64 CallbackId, const FString& ResultJson);

	/**
	 * Global callback dispatcher - allows multiple modules to register callback handlers
	 * @param CallbackId Callback ID
	 * @param ResultJson Result JSON string
	 * @return true if callback was handled by any module
	 */
	static bool DispatchGlobalCallback(int64 CallbackId, const FString& ResultJson);

	/**
	 * Register a global callback handler (for other modules like TapLogin)
	 * @param Handler Function that attempts to trigger a callback, returns true if handled
	 */
	static void RegisterGlobalCallbackHandler(TFunction<bool(int64, const FString&)> Handler);

private:
	/** Global callback handlers from all modules */
	static TArray<TFunction<bool(int64, const FString&)>> GlobalCallbackHandlers;

private:
	/** Initialize JNI environment */
	void InitializeJNI();

	/** Call Android Native SDK init method via Router */
	void CallAndroidInit(const FTapTapSdkOptions& Config);

	/** Apply screen orientation to the Android Activity via JNI */
	void ApplyScreenOrientation(int32 Orientation);

	/** Convert FTapTapSdkOptions to JSON string for JNI */
	FString ConfigToJson(const FTapTapSdkOptions& Config) const;

	/** Create Router request JSON */
	static FString CreateRouteRequest(const FString& MethodName, const FString& ParamsJson);

	/** Parse Router response JSON */
	static bool ParseRouteResponse(const FString& ResponseJson, FString& OutData, FString& OutMessage);

	/** Initialized flag */
	bool bIsInitialized;

	/** Callback registry - maps callback ID to callback function */
	static TMap<int64, TFunction<void(const FString&)>> CallbackRegistry;

	/** Callback registry lock */
	static FCriticalSection CallbackLock;
};

#endif // PLATFORM_ANDROID

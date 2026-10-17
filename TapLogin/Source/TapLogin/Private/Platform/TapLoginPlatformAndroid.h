// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Platform/ITapLoginPlatform.h"

#if PLATFORM_ANDROID

// Explicit JNI include needed when PCH is disabled (NoPCHs for Android);
// jobject / JNIEnv would otherwise only be available via PCH injection.
#include "Android/AndroidJNI.h"

/**
 * Android 平台登录实现
 * 通过 JNI 桥接调用 Android TapLogin SDK
 */
class FTapLoginPlatformAndroid : public ITapLoginPlatform
{
public:
	FTapLoginPlatformAndroid();
	virtual ~FTapLoginPlatformAndroid() override;

	// ITapLoginPlatform implementation
	virtual void Initialize() override;
	virtual bool IsInitialized() const override;
	virtual void LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback) override;
	virtual void Logout() override;
	virtual TOptional<FTapAccount> GetCurrentAccount() const override;
	virtual FString GetPlatformName() const override { return TEXT("Android"); }

public:
	// Router methods - Bridge to Android SDK
	/**
	 * Call TapLogin method synchronously through Router
	 * @param MethodName Router method name
	 * @param ParamsJson Optional parameters JSON string
	 * @return Result JSON string: {"success": true/false, "data": "...", "message": "..."}
	 */
	static FString CallModuleRoute(const FString& MethodName, const FString& ParamsJson = TEXT(""));

	/**
	 * Call TapLogin method asynchronously through Router
	 * @param MethodName Router method name
	 * @param ParamsJson Parameters JSON string
	 * @param Callback Callback function called when async operation completes
	 */
	static void CallModuleRouteAsync(
		const FString& MethodName,
		const FString& ParamsJson,
		TFunction<void(const FString&)> Callback);

	/** Release all pending JNI callback references during module/platform shutdown. */
	static void ShutdownCallbacks();

	// JNI Callback Management
	/**
	 * Create EngineBridgeCallback JNI object
	 * @param Env JNI environment
	 * @param Callback C++ callback function
	 * @param OutCallbackId ID registered for the callback
	 * @return Global reference to EngineBridgeCallbackProxy object
	 */
	static jobject CreateEngineBridgeCallback(
		JNIEnv* Env,
		TFunction<void(const FString&)> Callback,
		int64& OutCallbackId);

	/**
	 * Trigger and remove callback
	 * @param CallbackId Callback ID
	 * @param ResultJson Result JSON string
	 */
	static bool TriggerCallback(int64 CallbackId, const FString& ResultJson);

private:
	bool bInitialized;

	// JNI 辅助方法
	FString ScopeToString(ETapLoginScope Scope) const;
	TArray<FString> ScopesToStringArray(const TArray<ETapLoginScope>& Scopes) const;

	/** Create Router request JSON */
	static FString CreateRouteRequest(const FString& MethodName, const FString& ParamsJson);

	/** Parse Router response JSON */
	static bool ParseRouteResponse(const FString& ResponseJson, FString& OutData, FString& OutMessage);

	struct FCallbackEntry
	{
		TFunction<void(const FString&)> Callback;
		jobject ProxyGlobal = nullptr;
	};

	/** Callback registry owns both the C++ callback and its JNI global reference. */
	static TMap<int64, FCallbackEntry> CallbackRegistry;

	/** Callback registry lock */
	static FCriticalSection CallbackLock;

	/** Atomically remove a callback entry so exactly one path owns its cleanup. */
	static bool TakeCallbackEntry(int64 CallbackId, FCallbackEntry& OutEntry);

	/** Release the JNI reference owned by an entry. */
	static void ReleaseCallbackEntry(JNIEnv* Env, FCallbackEntry& Entry);

	/** Complete a request that failed before an asynchronous Java callback was accepted. */
	static void FailPendingCallback(JNIEnv* Env, int64 CallbackId, const FString& ResultJson);
};

#endif // PLATFORM_ANDROID

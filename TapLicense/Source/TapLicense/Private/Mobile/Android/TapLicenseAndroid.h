// Copyright TapTap. All Rights Reserved.
#pragma once

#if PLATFORM_ANDROID

#include "CoreMinimal.h"
#include "Platform/ITapLicensePlatform.h"
#include "Android/AndroidJNI.h"

/**
 * Android 实现：经专属 routeToTapLicense → TapLicenseUERouter 调原生 SDK。
 * 持久事件（license result / DLC order）通过 JNI nativeOn* 入口回调。
 * Per-call 回调（queryDLC）通过 EngineBridgeCallbackProxy + TapCore 全局分发。
 */
class FTapLicenseAndroid : public ITapLicensePlatform
{
public:
	FTapLicenseAndroid();

	virtual void SetupCallbacks(FLicenseSuccessFn, FLicenseFailedFn, FDLCQueryFn, FDLCOrderFn) override;
	virtual void Initialize() override;
	virtual void Shutdown() override;
	virtual void CheckLicense(bool bForce) override;
	virtual void QueryDLC(const TArray<FString>& DLCList) override;
	virtual void PurchaseDLC(const FString& DLCID) override;
	virtual FString GetPlatformName() const override { return TEXT("Android"); }

	/** JNI 持久回调入口：license 验证结果 */
	static void DispatchLicenseEvent(bool bOwned);
	/** JNI 持久回调入口：DLC 查询结果 */
	static void DispatchDLCQueryResult(const FString& ResultJson);
	/** JNI 持久回调入口：DLC 购买/状态变更 */
	static void DispatchDLCOrderEvent(const FString& Sku, int32 Status);
	/** TapCore 全局分发器回调（per-call，供 QueryDLC 使用） */
	static void TriggerPerCallCallback(int64 CallbackId, const FString& ResultJson);

	static FLicenseSuccessFn CbSuccess;
	static FLicenseFailedFn  CbFailed;
	static FDLCQueryFn       CbDLCQuery;
	static FDLCOrderFn       CbDLCOrder;
	static FCriticalSection  CallbackLock;

	static TMap<int64, TFunction<void(const FString&)>> PerCallRegistry;
	static FCriticalSection  PerCallLock;

private:
	/** 确保原生持久回调已注册；Java 侧实现幂等，可在每次 API 调用前安全确认。 */
	static bool EnsureNativeCallbacksRegistered();
	/** 同步路由：调 GameActivity.routeToTapLicense(json, null) */
	static FString CallModuleRoute(const FString& MethodName, const FString& ParamsJson);
	/** 异步路由：调 GameActivity.routeToTapLicense(json, callback) */
	static void CallModuleRouteAsync(const FString& MethodName, const FString& ParamsJson,
	                                 TFunction<void(const FString&)> Callback);

	static jobject CreateEngineBridgeCallback(JNIEnv* Env, TFunction<void(const FString&)> Callback);
	static void    DestroyEngineBridgeCallback(JNIEnv* Env, jobject CallbackObj);
	static FString CreateRouteRequest(const FString& MethodName, const FString& ParamsJson);
	static bool    ParseRouteResponse(const FString& ResponseJson, FString& OutData, FString& OutMessage);
};

#endif // PLATFORM_ANDROID

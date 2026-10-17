// Copyright TapTap. All Rights Reserved.

#pragma once

// Only compile on Android platform
#if PLATFORM_ANDROID

#include "CoreMinimal.h"
#include "Platform/ITapCompliancePlatform.h"
#include "Mobile/TapComplianceAsyncRequests.h"
#include <jni.h>

/**
 * Android Platform Implementation for TapCompliance
 * Uses JNI to bridge to Android TapCompliance SDK via Router pattern
 *
 * 回调架构：
 * - 全局合规事件（startup 触发的 500/1000/1001/1009/9002 等）：
 *   通过专用 JNI native 方法 nativeOnComplianceEvent 回调（持久，不消耗）
 *   C++ 实现: Java_com_taptap_sdk_compliance_internal_ue_TapComplianceUERouter_nativeOnComplianceEvent
 *
 * - 支付相关（checkPaymentLimit / submitPayment）：
 *   通过 EngineBridgeCallback 一次性回调，与 TapCore/TapLogin 共享 nativeOnResult 入口
 */
class FTapComplianceAndroid : public ITapCompliancePlatform
{
public:
	FTapComplianceAndroid();
	virtual ~FTapComplianceAndroid();

	// ITapCompliancePlatform interface
	virtual void ApplyInitOptions(const FTapComplianceOptions& Options) override;
	virtual void RegisterComplianceCallback(TFunction<void(int32, const FString&)> Callback) override;
	virtual void UnregisterComplianceCallback() override;
	virtual void Startup(const FString& UserId) override;
	virtual void Exit() override;
	virtual void CheckPaymentLimit(
		int64 Amount,
		TFunction<void(int32, const FString&, const FString&)> Callback,
		TFunction<void(const FString&)> OnError) override;
	virtual void SubmitPayment(
		int64 Amount,
		TFunction<void(bool, const FString&)> Callback) override;
	virtual int32 GetAgeRange() override;
	virtual int32 GetUserRemainTime() override;
	virtual FString GetCurrentAccessToken() override;
	virtual FString GetPlatformName() const override { return TEXT("Android"); }

	// Router methods - Internal infrastructure for Android SDK bridge
public:
	/**
	 * Call TapCompliance method synchronously through Router
	 * @param MethodName Router method name
	 * @param ParamsJson Optional parameters JSON string
	 * @return Result JSON string: {"success": true/false, "data": "...", "message": "..."}
	 */
	static FString CallModuleRoute(const FString& MethodName, const FString& ParamsJson = TEXT(""));

	/**
	 * Call TapCompliance method asynchronously through Router
	 * @param MethodName Router method name
	 * @param ParamsJson Parameters JSON string
	 * @param Callback Callback function called when async operation completes
	 */
	static void CallModuleRouteAsync(
		const FString& MethodName,
		const FString& ParamsJson,
		TFunction<void(const FString&)> Callback);

	/**
	 * 分发合规事件 - 由 JNI native 方法调用（persistent callback）
	 * Java 侧: TapComplianceUERouter.nativeOnComplianceEvent(int code, String extraJson)
	 */
	static void DispatchComplianceEvent(int32 Code, const FString& ExtraJson);

private:
	/** Returns a local reference; the pending request owns a separate global reference. */
	static jobject CreateEngineBridgeCallback(JNIEnv* Env, int64 CallbackId);

	/** Create Router request JSON */
	static FString CreateRouteRequest(const FString& MethodName, const FString& ParamsJson);

	/** Parse Router response JSON (standard format: {"success":true/false,"data":...,"message":...}) */
	static bool ParseRouteResponse(const FString& ResponseJson, FString& OutData, FString& OutMessage);

	/** 全局合规事件回调（persistent，注册后持续有效） */
	static TFunction<void(int32, const FString&)> ComplianceEventCallback;
	static FCriticalSection ComplianceCallbackLock;

	/** Per-call 回调注册表（checkPaymentLimit / submitPayment，触发后移除） */
	static FTapComplianceAsyncRequests PendingRequests;
};

#endif // PLATFORM_ANDROID

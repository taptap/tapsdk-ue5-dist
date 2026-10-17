// Copyright TapTap. All Rights Reserved.

#pragma once

// Only compile on iOS platform
#if PLATFORM_IOS

#include "CoreMinimal.h"
#include "Platform/ITapCompliancePlatform.h"
#include "Templates/Atomic.h"

/**
 * iOS Platform Implementation for TapCompliance
 * Uses Objective-C++ to bridge to iOS Native TapTapComplianceSDK
 *
 * 回调架构：
 * - 全局合规事件 (RegisterComplianceCallback) - 通过 ObjC delegate 持久回调
 * - 支付相关 (CheckPaymentLimit/SubmitPayment) - 通过 ObjC completion block 一次性回调
 */
class FTapComplianceiOS : public ITapCompliancePlatform
{
public:
	FTapComplianceiOS();
	virtual ~FTapComplianceiOS();

	// ITapCompliancePlatform interface
	virtual void ApplyInitOptions(const FTapComplianceOptions& Options) override;
	virtual void RegisterComplianceCallback(TFunction<void(int32 /*Code*/, const FString& /*ExtraJson*/)> Callback) override;
	virtual void UnregisterComplianceCallback() override;
	virtual void Startup(const FString& UserId) override;
	virtual void Exit() override;
	virtual void CheckPaymentLimit(
		int64 Amount,
		TFunction<void(int32 /*Status*/, const FString& /*Title*/, const FString& /*Description*/)> Callback,
		TFunction<void(const FString&)> OnError) override;
	virtual void SubmitPayment(
		int64 Amount,
		TFunction<void(bool /*bSuccess*/, const FString& /*Message*/)> Callback) override;
	virtual int32 GetAgeRange() override;
	virtual int32 GetUserRemainTime() override;
	virtual FString GetCurrentAccessToken() override;
	virtual FString GetPlatformName() const override { return TEXT("iOS"); }

	/**
	 * 供 ObjC bridge 调用 - 分发合规事件到已注册的 C++ 回调
	 * Called by TapComplianceIOSBridge when compliance event is received
	 */
	void DispatchComplianceEvent(int32 Code, const FString& ExtraJson);

	/** 使异步原生回调失效，避免回调访问已销毁的 C++ 实例。 */
	void InvalidateCallbacks();

private:
	TSharedRef<TAtomic<bool>> CallbackAlive;
	/** 持久合规事件回调（RegisterComplianceCallback 注册） */
	TFunction<void(int32, const FString&)> ComplianceEventCallback;
	FCriticalSection ComplianceCallbackLock;
};

#endif // PLATFORM_IOS

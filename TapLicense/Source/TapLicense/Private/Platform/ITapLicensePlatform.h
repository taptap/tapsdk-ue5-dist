// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapLicenseTypes.h"

/**
 * TapLicense 平台接口。所有平台实现必须继承此接口。
 * 对齐 TapCompliance 的 ITapCompliancePlatform + 工厂范式。
 *
 * 生命周期：SetupCallbacks() 注入回调 → Initialize() 注册原生事件 → Shutdown()。
 * 所有用户回调最终在 GameThread 触发。
 */
class ITapLicensePlatform
{
public:
	virtual ~ITapLicensePlatform() = default;

	using FLicenseSuccessFn = TFunction<void()>;
	using FLicenseFailedFn  = TFunction<void()>;
	using FDLCQueryFn       = TFunction<void(ETapLicenseQueryCode, const FTapLicenseDLCResultMap&)>;
	using FDLCOrderFn       = TFunction<void(const FString&, ETapDLCPurchasedCode)>;

	/** 注入持久回调（门面把 UTapLicense 的静态 delegate Broadcast 包成 TFunction）。 */
	virtual void SetupCallbacks(
		FLicenseSuccessFn OnSuccess,
		FLicenseFailedFn  OnFailed,
		FDLCQueryFn       OnDLCQuery,
		FDLCOrderFn       OnDLCOrder) = 0;

	/** 注册原生事件 / 启动。Win64 注册 EventId 4001/4002；移动端注册原生回调。 */
	virtual void Initialize() = 0;

	/** 注销并释放。模块 ShutdownModule 调用。 */
	virtual void Shutdown() = 0;

	virtual void CheckLicense(bool bForce) = 0;
	virtual void QueryDLC(const TArray<FString>& DLCList) = 0;
	virtual void PurchaseDLC(const FString& DLCID) = 0;

	/** 平台名（日志用）。 */
	virtual FString GetPlatformName() const = 0;
};

/** TapLicense 平台工厂（单例）。 */
class TAPLICENSE_API FTapLicensePlatformFactory
{
public:
	static TSharedPtr<ITapLicensePlatform> CreatePlatform();
	/** 返回已创建的单例（不创建）。Shutdown 用，避免在未初始化时凭空构造。 */
	static TSharedPtr<ITapLicensePlatform> GetExisting();
	/** 释放单例（ShutdownModule 调用，便于测试复位）。 */
	static void ResetPlatform();

private:
	static TSharedPtr<ITapLicensePlatform> PlatformInstance;
};

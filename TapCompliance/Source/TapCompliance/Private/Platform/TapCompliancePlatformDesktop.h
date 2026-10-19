// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Platform/ITapCompliancePlatform.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

/**
 * Desktop 平台合规实现
 * Windows/Mac/Linux 通过 PC 合规 HTTP、实名面板、心跳轮询与本地缓存实现。
 */
class FTapCompliancePlatformDesktop : public ITapCompliancePlatform
{
public:
	FTapCompliancePlatformDesktop() = default;
	virtual ~FTapCompliancePlatformDesktop() override = default;

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
	virtual FString GetPlatformName() const override { return TEXT("Desktop"); }
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

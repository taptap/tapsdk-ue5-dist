// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapComplianceOptions.h"

/**
 * Platform Interface for TapCompliance
 * All platform-specific implementations must inherit from this interface
 *
 * 全局合规事件通过 RegisterComplianceCallback 注册的持久回调传递
 * 支付相关查询通过对应异步回调返回
 */
class ITapCompliancePlatform
{
public:
	virtual ~ITapCompliancePlatform() = default;

	/** Apply module-owned initialization options before any Compliance API is used. */
	virtual void ApplyInitOptions(const FTapComplianceOptions& Options) = 0;

	/**
	 * 注册全局合规事件回调（持久，多次触发）
	 * 对应 Android SDK: TapTapCompliance.registerComplianceCallback()
	 *
	 * 事件码:
	 *   500  = LOGIN_SUCCESS 认证成功，可以进入游戏
	 *   1000 = EXITED 用户退出 / Token 过期
	 *   1001 = 切换账号
	 *   1030 = 时段 / 时长限制
	 *   1100 = 内容分级不通过
	 *   1200 = 配置无效 / 网络错误
	 *   9002 = 实名认证失败
	 *
	 * @param Callback (Code, ExtraJson) - 事件码 + 附加信息 JSON
	 */
	virtual void RegisterComplianceCallback(TFunction<void(int32 /*Code*/, const FString& /*ExtraJson*/)> Callback) = 0;

	/**
	 * 注销全局合规事件回调
	 * 对应 Android SDK: TapTapCompliance.unregisterComplianceCallback()
	 */
	virtual void UnregisterComplianceCallback() = 0;

	/**
	 * 启动合规认证流程
	 * 对应 Android SDK: TapTapCompliance.startup()
	 *
	 * @param UserId 用户 ID
	 */
	virtual void Startup(const FString& UserId) = 0;

	/**
	 * 退出合规 UI
	 * 对应 Android SDK: TapTapCompliance.exit()
	 */
	virtual void Exit() = 0;

	/**
	 * 检查支付限额（异步）
	 * 对应 Android SDK: TapTapCompliance.checkPaymentLimit()
	 *
	 * @param Amount 支付金额（单位：分）
	 * @param Callback (Status, Title, Description) - 支付状态（1=允许，0=受限）+ 提示标题 + 提示描述
	 */
	virtual void CheckPaymentLimit(
		int64 Amount,
		TFunction<void(int32 /*Status*/, const FString& /*Title*/, const FString& /*Description*/)> Callback,
		TFunction<void(const FString&)> OnError) = 0;

	/**
	 * 提交支付结果（异步，fire-and-forget）
	 * 对应 Android SDK: TapTapCompliance.submitPayment()
	 *
	 * @param Amount 实际支付金额（单位：分）
	 * @param Callback (bSuccess, Message) - 是否上报成功 + 错误信息
	 */
	virtual void SubmitPayment(
		int64 Amount,
		TFunction<void(bool /*bSuccess*/, const FString& /*Message*/)> Callback) = 0;

	/**
	 * 获取用户年龄段（同步）
	 * 对应 Android SDK: TapTapCompliance.getAgeRange()
	 *
	 * @return -1 = 未知, 0 = 0岁以下, 8 = 8岁以下, 16 = 16岁以下, 18 = 成年人；未授权年龄哨兵值不会对外返回
	 */
	virtual int32 GetAgeRange() = 0;

	/**
	 * 获取用户剩余游戏时间（同步）
	 * 对应 Android SDK: TapTapCompliance.getRemainingTime()
	 *
	 * @return 剩余游戏时间（秒），-1 = 未知
	 */
	virtual int32 GetUserRemainTime() = 0;

	/**
	 * 获取当前用户合规 AccessToken（同步）
	 * @return anti_addiction_token，未登录或平台不支持时为空
	 */
	virtual FString GetCurrentAccessToken() = 0;

	/**
	 * 获取平台名称（用于日志）
	 */
	virtual FString GetPlatformName() const = 0;
};

/**
 * TapCompliance 平台工厂（单例）
 */
class FTapCompliancePlatformFactory
{
public:
	static TSharedPtr<ITapCompliancePlatform> CreatePlatform();

private:
	static TSharedPtr<ITapCompliancePlatform> PlatformInstance;
};

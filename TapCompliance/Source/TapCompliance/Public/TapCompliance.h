// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "TapCompliance.generated.h"

/**
 * TapCompliance 合规事件回调接口
 *
 * 通过 FTapCompliance::RegisterComplianceCallback 注册
 * 持久回调，每次合规状态变化时触发
 */
UINTERFACE(MinimalAPI, Blueprintable)
class UTapComplianceCallback : public UInterface
{
	GENERATED_BODY()
};

class TAPCOMPLIANCE_API ITapComplianceCallback
{
	GENERATED_BODY()

public:
	/**
	 * 合规事件回调
	 * C++ 实现方请重写 OnComplianceResult_Implementation；蓝图实现会通过 Execute_OnComplianceResult 分发。
	 *
	 * @param Code 事件码（对应 Android ComplianceMessage / Unity StartUpResult）:
	 *   500  = LOGIN_SUCCESS                   认证成功，可以进入游戏
	 *   1000 = EXITED                         用户退出或 Token 过期
	 *   1001 = SWITCH_ACCOUNT                 切换账号
	 *   1030 = PERIOD_RESTRICT                时段或时长限制
	 *   1100 = AGE_LIMIT                      内容分级不通过
	 *   1200 = INVALID_CLIENT_OR_NETWORK_ERROR 配置无效或网络错误
	 *   9002 = REAL_NAME_STOP                 实名流程被用户关闭
	 * @param ExtraJson 附加信息（JSON 字符串，可为空）
	 */
	UFUNCTION(BlueprintNativeEvent, Category="TapCompliance")
	void OnComplianceResult(int32 Code, const FString& ExtraJson);
};

/**
 * TapCompliance 公共 API
 * 防沉迷实名认证模块
 */
class TAPCOMPLIANCE_API FTapCompliance
{
public:
	// =========================================================
	// 核心流程
	// =========================================================

	/**
	 * 注册全局合规事件回调（必须在 Startup 之前调用）
	 * 持久回调，多次触发，对应 Android SDK: TapTapCompliance.registerComplianceCallback()
	 *
	 * @param Callback 合规事件回调接口
	 */
	static void RegisterComplianceCallback(TScriptInterface<ITapComplianceCallback> Callback);

	/**
	 * 注销全局合规事件回调
	 * 对应 Android SDK: TapTapCompliance.unregisterComplianceCallback()
	 */
	static void UnregisterComplianceCallback(TScriptInterface<ITapComplianceCallback> Callback);

	/**
	 * 启动合规认证流程
	 * 对应 Android SDK: TapTapCompliance.startup()
	 *
	 * @param UserId 用户 ID
	 */
	static void Startup(const FString& UserId);

	/**
	 * 退出合规 UI
	 * 对应 Android SDK: TapTapCompliance.exit()
	 */
	static void Exit();

	// =========================================================
	// 支付相关
	// =========================================================

	/**
	 * 检查支付限额（异步）
	 * 对应 Android SDK: TapTapCompliance.checkPaymentLimit()
	 *
	 * @param Amount 支付金额（单位：分）
	 * @param Callback (Status, Title, Description):
	 *   Status=1: 允许支付
	 *   Status=0: 支付受限，Title/Description 为提示内容
	 * @param OnError 检查未完成时的错误回调，不作为支付受限结果返回。
	 */
	static void CheckPaymentLimit(
		int64 Amount,
		TFunction<void(int32 /*Status*/, const FString& /*Title*/, const FString& /*Description*/)> Callback,
		TFunction<void(const FString&)> OnError);

	/**
	 * 提交支付结果（异步）
	 * 支付成功后调用，上报支付金额到合规系统
	 * 对应 Android SDK: TapTapCompliance.submitPayment()
	 *
	 * @param Amount 实际支付金额（单位：分）
	 * @param Callback (bSuccess, Message) - 上报结果
	 */
	static void SubmitPayment(
		int64 Amount,
		TFunction<void(bool /*bSuccess*/, const FString& /*Message*/)> Callback);

	// =========================================================
	// 用户信息查询（同步）
	// =========================================================

	/**
	 * 获取用户年龄段（同步）
	 * 对应 Android SDK: TapTapCompliance.getAgeRange()
	 *
	 * @return -1=未知, 0=0岁以下, 8=8岁以下, 16=16岁以下, 18=成年人；未授权年龄哨兵值不会对外返回
	 */
	static int32 GetAgeRange();

	/**
	 * 获取用户剩余游戏时间（同步）
	 * 对应 Android SDK: TapTapCompliance.getRemainingTime()
	 *
	 * @return 剩余游戏时间（秒），-1=未知
	 */
	static int32 GetRemainingTime();

	/**
	 * 获取当前用户合规 AccessToken（同步，仅桌面端 Windows/Mac/Linux 生效）
	 * 桌面端直接读取合规模块内部状态（FComplianceState），不经过平台抽象层。
	 *
	 * @return AccessToken 字符串，空字符串表示未登录或非桌面端
	 */
	static FString GetCurrentAccessToken();

};

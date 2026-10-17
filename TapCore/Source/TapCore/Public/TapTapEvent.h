// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * TapSDK 事件追踪类
 * 对齐 Unity 的 TapTapEvent 接口
 * 用于数据分析、用户行为追踪和事件上报
 */
class TAPCORE_API FTapTapEvent
{
public:
	/**
	 * 设置用户 ID
	 * @param UserID 用户唯一标识符
	 */
	static void SetUserID(const FString& UserID);

	/**
	 * 设置用户 ID (带属性)
	 * @param UserID 用户唯一标识符
	 * @param Properties JSON 格式的用户属性
	 */
	static void SetUserID(const FString& UserID, const FString& Properties);

	/**
	 * 清除当前用户
	 */
	static void ClearUser();

	/**
	 * 获取设备 ID
	 * @return 设备唯一标识符
	 */
	static FString GetDeviceId();

	/**
	 * 记录自定义事件
	 * @param Name 事件名称
	 * @param Properties JSON 格式的事件属性
	 */
	static void LogEvent(const FString& Name, const FString& Properties);

	/**
	 * 初始化设备属性
	 * @param Properties JSON 格式的设备属性
	 */
	static void DeviceInitialize(const FString& Properties);

	/**
	 * 更新设备属性
	 * @param Properties JSON 格式的设备属性
	 */
	static void DeviceUpdate(const FString& Properties);

	/**
	 * 累加设备属性
	 * @param Properties JSON 格式的设备属性
	 */
	static void DeviceAdd(const FString& Properties);

	/**
	 * 初始化用户属性
	 * @param Properties JSON 格式的用户属性
	 */
	static void UserInitialize(const FString& Properties);

	/**
	 * 更新用户属性
	 * @param Properties JSON 格式的用户属性
	 */
	static void UserUpdate(const FString& Properties);

	/**
	 * 累加用户属性
	 * @param Properties JSON 格式的用户属性
	 */
	static void UserAdd(const FString& Properties);

	/**
	 * 添加公共属性 (单个)
	 * @param Key 属性键
	 * @param Value 属性值
	 */
	static void AddCommonProperty(const FString& Key, const FString& Value);

	/**
	 * 添加公共属性 (批量)
	 * @param Properties JSON 格式的公共属性
	 */
	static void AddCommon(const FString& Properties);

	/**
	 * 清除单个公共属性
	 * @param Key 属性键
	 */
	static void ClearCommonProperty(const FString& Key);

	/**
	 * 清除多个公共属性
	 * @param Keys 属性键数组
	 */
	static void ClearCommonProperties(const TArray<FString>& Keys);

	/**
	 * 清除所有公共属性
	 */
	static void ClearAllCommonProperties();

	/**
	 * 记录购买事件
	 * 对齐 Unity TapTapEvent.LogPurchasedEvent(orderID, productName, amount, currencyType, paymentMethod, properties)
	 */
	static void LogPurchasedEvent(
		const FString& OrderID,
		const FString& ProductName,
		int64 Amount,
		const FString& CurrencyType,
		const FString& PaymentMethod,
		const FString& Properties = TEXT(""));

	/**
	 * 注册动态属性回调
	 * @param Callback 返回 JSON 格式动态属性的回调函数
	 */
	static void RegisterDynamicProperties(TFunction<FString()> Callback);

	/**
	 * 设置 OAID (Android 广告标识符)
	 * @param Value OAID 值
	 */
	static void SetOAID(const FString& Value);

	/**
	 * 记录设备登录事件
	 */
	static void LogDeviceLoginEvent();

private:
	friend class FTapDBInternal;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FTapEventInitializationTest;
#endif

	/** Internal initialization used by FTapDBInternal during TapSDK Init(). */
	static void Init(bool bEnableLog);
	static bool CanUseEventApi();

	/** 是否已初始化 */
	static bool bIsInitialized;

	/** 是否启用日志 */
	static bool bEnableLog;

	/** 动态属性回调 */
	static TFunction<FString()> DynamicPropertiesCallback;

	/** 平台实现 */
	static TUniquePtr<class ITapEventPlatform> PlatformImpl;

	/** 创建平台实现 */
	static void CreatePlatformImpl();
};

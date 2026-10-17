// Copyright TapTap SDK Team. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * TapCore Router Method Names
 *
 * 定义所有路由方法的名称常量，与 Android 端的 RouteMethod 枚举对应
 * 确保 C++ 和 Kotlin 端使用相同的方法名称字符串
 *
 * Android 对应类: com.taptap.sdk.core.internal.ue.TapCoreUERouter.RouteMethod
 *
 * @note 修改方法名时，必须同时更新 Android 端的 RouteMethod 枚举
 * @see TapCoreUERouter.kt
 */
namespace TapCore
{
	namespace RouterMethods
	{
		/**
		 * 初始化 SDK
		 *
		 * - 需要 Activity: 是
		 * - 需要 Params: 是
		 * - 参数格式: TapConfig JSON
		 */
		constexpr const TCHAR* INIT = TEXT("init");

		/**
		 * 检查初始化状态
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 否
		 * - 返回: {"success": true, "data": "true/false"}
		 */
		constexpr const TCHAR* IS_INITIALIZED = TEXT("isInitialized");

		/**
		 * 更新语言设置
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 是
		 * - 参数格式: 语言代码整数字符串 (0=Auto, 1=zh_Hans, 2=en, 3=zh_Hant, 4=ja, 5=ko)
		 */
		constexpr const TCHAR* UPDATE_LANGUAGE = TEXT("updateLanguage");

		/**
		 * 发送业务日志
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 是
		 * - 参数格式: {"project": "...", "version": "...", "action": "...", "properties": "..."}
		 */
		constexpr const TCHAR* SEND_OPEN_LOG = TEXT("sendOpenLog");

		/**
		 * 设置用户 ID
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 是
		 * - 参数格式: {"userId": "...", "properties": "..."}
		 */
		constexpr const TCHAR* SET_USER_ID = TEXT("setUserId");

		/**
		 * 清除用户信息
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 否
		 */
		constexpr const TCHAR* CLEAR_USER = TEXT("clearUser");

		/**
		 * 获取设备 ID
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 否
		 * - 返回: {"success": true, "data": "device_id"}
		 */
		constexpr const TCHAR* GET_DEVICE_ID = TEXT("getDeviceId");

		/**
		 * 记录事件
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 是
		 * - 参数格式: {"eventName": "...", "properties": "..."}
		 */
		constexpr const TCHAR* LOG_EVENT = TEXT("logEvent");

		/**
		 * 记录购买事件
		 *
		 * - 需要 Activity: 否
		 * - 需要 Params: 是
		 * - 参数格式: Order JSON (包含 orderId, productName, amount, currencyType, paymentMethod, properties)
		 */
		constexpr const TCHAR* LOG_PURCHASE_EVENT = TEXT("logPurchaseEvent");

		/**
		 * 获取所有支持的方法名称列表
		 *
		 * @return 所有方法名称的数组
		 */
		inline TArray<FString> GetAllMethodNames()
		{
			return {
				INIT,
				IS_INITIALIZED,
				UPDATE_LANGUAGE,
				SEND_OPEN_LOG,
				SET_USER_ID,
				CLEAR_USER,
				GET_DEVICE_ID,
				LOG_EVENT,
				LOG_PURCHASE_EVENT
			};
		}

		/**
		 * 验证方法名是否有效
		 *
		 * @param MethodName 要验证的方法名
		 * @return 如果是有效的方法名返回 true
		 */
		inline bool IsValidMethodName(const FString& MethodName)
		{
			return GetAllMethodNames().Contains(MethodName);
		}
	}
}

/**
 * 使用示例:
 *
 * @code
 * // 调用 init 方法
 * FString Result = CallRoute(TapCore::RouterMethods::INIT, ConfigJson);
 *
 * // 检查初始化状态
 * FString Result = CallRoute(TapCore::RouterMethods::IS_INITIALIZED);
 *
 * // 更新语言
 * FString LanguageCode = FString::FromInt(static_cast<int32>(ETapLanguage::English));
 * FString Result = CallRoute(TapCore::RouterMethods::UPDATE_LANGUAGE, LanguageCode);
 *
 * // 验证方法名
 * if (TapCore::RouterMethods::IsValidMethodName(TEXT("init")))
 * {
 *     // 有效的方法名
 * }
 * @endcode
 */

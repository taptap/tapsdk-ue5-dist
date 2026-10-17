// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTapSdkChecker, Log, All);

/**
 * SDK 初始化状态检查器
 * 对齐 Android 端 TapSdkChecker
 * 用于在调用业务接口前检查 SDK 是否已正确初始化
 * 支持所有平台 (Android/iOS/Windows/Mac/Linux)
 */
class TAPCORE_API FTapSdkChecker
{
public:
	/**
	 * 检查 SDK 是否已初始化
	 * 对齐 Unity TapCoreStandalone.CheckInitState()
	 * 检查项：1) HTTP 已初始化  2) ClientInfo 匹配（gatekeeper 未返回 invalid_client）
	 * @param bShowNotice 未初始化时是否在屏幕上显示提示信息
	 * @return 是否已初始化
	 */
	static bool CheckInitialize(bool bShowNotice = true);

	/**
	 * SDK 内部使用：本地同步初始化是否已开始。InProgress 视为通过，不要求 Gatekeeper 成功。
	 * 对齐 Unity CheckLocalInitState / Android checkLocalInitialize。
	 */
	static bool CheckLocalInitialize(bool bShowNotice = false);

	/**
	 * 检查指定模块是否已加载
	 * @param ModuleName UE 模块名（如 "TapLogin", "TapCompliance" 等）
	 * @return 模块是否已加载
	 */
	static bool CheckModule(const FString& ModuleName);

	/**
	 * 设置 ClientInfo 匹配状态
	 * 对齐 Unity isClientInfoMatched
	 * gatekeeper 返回 ERROR_INVALID_CLIENT 时调用 SetClientInfoMatched(false)
	 */
	static void SetClientInfoMatched(bool bMatched);

	/**
	 * 更新 SDK 最终初始化状态。TapCore 在 Init 开始、Gatekeeper 终态时调用。
	 * bInProgress 为 true 时，业务 API 应提示“SDK 正在初始化中”。
	 */
	static void SetInitializationState(bool bInitialized, bool bInProgress);

private:
	/** ClientInfo 是否与服务器匹配，默认 true，gatekeeper 返回 invalid_client 时置 false */
	static bool bIsClientInfoMatched;

	/** SDK 是否已经完成最终初始化（Gatekeeper 成功或网络降级成功） */
	static bool bIsSdkInitialized;

	/** SDK 是否正在等待最终初始化结果 */
	static bool bIsInitializationInProgress;
};

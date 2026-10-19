// Copyright TapTap. All Rights Reserved.

#include "Internal/TapSdkChecker.h"
#include "TapMessage.h"
#include "Internal/Http/TapHttp.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogTapSdkChecker);

// 对齐 Unity isClientInfoMatched，默认 true
bool FTapSdkChecker::bIsClientInfoMatched = true;
bool FTapSdkChecker::bIsSdkInitialized = false;
bool FTapSdkChecker::bIsInitializationInProgress = false;

bool FTapSdkChecker::CheckInitialize(bool bShowNotice)
{
	if (bIsInitializationInProgress)
	{
		UE_LOG(LogTapSdkChecker, Error, TEXT("SDK 正在初始化中，请稍后再试"));
		if (bShowNotice)
		{
			FTapMessage::ShowMessage(TEXT("SDK 正在初始化中，请稍后再试"));
		}
		return false;
	}

	// 配置错误优先于“未初始化”提示，保持 Android/Unity 的错误语义。
	if (!bIsClientInfoMatched)
	{
		UE_LOG(LogTapSdkChecker, Error,
			TEXT("当前应用初始化信息错误: 请在 TapTap 开发者中心检查当前应用调用初始化接口设置的 clientId、clientToken 是否匹配"));

		if (bShowNotice)
		{
			FTapMessage::ShowMessage(TEXT("当前应用初始化信息错误"));
		}

		return false;
	}

	// 检查 1：SDK 是否已完成最终初始化
	if (!bIsSdkInitialized || !FTapHttp::IsInitialized())
	{
		UE_LOG(LogTapSdkChecker, Error,
			TEXT("当前应用还未初始化: 请在调用 SDK 业务接口前，先调用 FTapTapSDK::Init()"));

		if (bShowNotice)
		{
			FTapMessage::ShowMessage(TEXT("当前应用还未初始化"));
		}

		return false;
	}

	return true;
}

bool FTapSdkChecker::CheckLocalInitialize(bool bShowNotice)
{
	if (bIsInitializationInProgress)
	{
		return true;
	}

	if (!bIsClientInfoMatched)
	{
		UE_LOG(LogTapSdkChecker, Error,
			TEXT("当前应用初始化信息错误: 请在 TapTap 开发者中心检查当前应用调用初始化接口设置的 clientId、clientToken 是否匹配"));
		if (bShowNotice)
		{
			FTapMessage::ShowMessage(TEXT("当前应用初始化信息错误"));
		}
		return false;
	}

	if (!bIsSdkInitialized || !FTapHttp::IsInitialized())
	{
		UE_LOG(LogTapSdkChecker, Error,
			TEXT("当前应用还未初始化: 请在调用 SDK 业务接口前，先调用 FTapTapSDK::Init()"));
		if (bShowNotice)
		{
			FTapMessage::ShowMessage(TEXT("当前应用还未初始化"));
		}
		return false;
	}

	return true;
}

void FTapSdkChecker::SetClientInfoMatched(bool bMatched)
{
	bIsClientInfoMatched = bMatched;
}

void FTapSdkChecker::SetInitializationState(bool bInitialized, bool bInProgress)
{
	bIsSdkInitialized = bInitialized;
	bIsInitializationInProgress = bInProgress;
}

bool FTapSdkChecker::CheckModule(const FString& ModuleName)
{
	if (FModuleManager::Get().IsModuleLoaded(*ModuleName))
	{
		return true;
	}

	UE_LOG(LogTapSdkChecker, Error,
		TEXT("模块 %s 未找到, 请确认已添加对应插件依赖"), *ModuleName);

	return false;
}

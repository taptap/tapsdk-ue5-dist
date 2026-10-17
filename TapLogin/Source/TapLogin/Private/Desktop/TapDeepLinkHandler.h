// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

DECLARE_DELEGATE_ThreeParams(FOnDeepLinkReceived, const FString& /*Code*/, const FString& /*State*/, const FString& /*Error*/);

/**
 * 处理 open-taptap-{clientId}:// URL Scheme 回调
 * Mac: 注册 kAEGetURL Apple Event
 * Windows: 注册用户级 URL Protocol，并从启动参数读取回调 URL
 */
class FTapDeepLinkHandler
{
public:
	static FTapDeepLinkHandler& Get();

	/** 注册 URL Scheme 监听 */
	void StartListening(const FString& ClientID, FOnDeepLinkReceived OnReceived);

	/** 停止监听 */
	void StopListening();

	/** 处理收到的 URL (内部使用) */
	void HandleURL(const FString& URL);

#if PLATFORM_WINDOWS
	/** 校验 Windows URL Protocol 回调并提取 ClientID，不读取或修改系统状态。 */
	static bool TryParseWindowsProtocolURL(const FString& URL, FString& OutClientID);
#endif

private:
	FTapDeepLinkHandler();
	~FTapDeepLinkHandler();

	bool bListening = false;
	FString ListenClientID;
	FOnDeepLinkReceived DeepLinkCallback;

	/** 从 URL 中解析 query 参数 */
	static TMap<FString, FString> ParseQueryParams(const FString& QueryString);

#if PLATFORM_WINDOWS
	static bool ParseCommandLineDeepLink(FString& OutURL, FString& OutClientID);
#endif

#if PLATFORM_MAC
	void RegisterAppleEventHandler();
	void UnregisterAppleEventHandler();
	id AppleEventHandler = nil;
#endif
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

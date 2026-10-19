// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

/**
 * 登录面板本地化字符串
 * 参照 Unity LoginLanguage.cs, 支持 15 种语言 (与 ETapLanguage 对应)
 */
class FTapLoginStrings
{
public:
	static FString TitleLogin();
	static FString QrTitleLogin();
	static FString QrRefresh();
	static FString QrNoticeScanToLogin();
	static FString WebLogin();
	static FString WebNotice();
	static FString WebButtonJumpToWeb();
	static FString QrNoticeCancelTitle();
	static FString QrNoticeCancelDetail();
	static FString QrNoticeSuccessTitle();
	static FString QrNoticeSuccessDetail();
	static FString WebNoticeLoginTitle();
	static FString WebNoticeFailTitle();
	static FString WebNoticeFailDetail();
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

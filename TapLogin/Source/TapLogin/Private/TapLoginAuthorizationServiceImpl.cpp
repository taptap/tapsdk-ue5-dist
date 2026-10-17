// Copyright TapTap. All Rights Reserved.

#include "TapLoginAuthorizationServiceImpl.h"
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"
#include "Internal/Http/TapHttpTime.h"

FString FTapLoginAuthorizationServiceImpl::ObtainAuthorization(
	const FString& Url, const FString& Method)
{
	// 拆 URL 为 host:port:path+query
	FString Host, Port, Path;
	Port = TEXT("443");

	FString Tmp = Url;
	if (Tmp.StartsWith(TEXT("https://")))
	{
		Tmp.RightChopInline(8, EAllowShrinking::No);
		Port = TEXT("443");
	}
	else if (Tmp.StartsWith(TEXT("http://")))
	{
		Tmp.RightChopInline(7, EAllowShrinking::No);
		Port = TEXT("80");
	}

	int32 SlashIdx = INDEX_NONE;
	Tmp.FindChar(TEXT('/'), SlashIdx);
	if (SlashIdx == INDEX_NONE)
	{
		Host = Tmp;
		Path = TEXT("/");
	}
	else
	{
		Host = Tmp.Left(SlashIdx);
		Path = Tmp.Mid(SlashIdx);
	}

	// 用校准过的时钟拿 timestamp; invalid_time 重试时会重新拿到新值
	int64 Ts = FTapHttpTime::GetCurrentTime();

	// 委托 TapLogin (现有实现)
	return FTapLoginInternal::BuildAuthorizationHeader(Method, Path, Host, Port, Ts);
}

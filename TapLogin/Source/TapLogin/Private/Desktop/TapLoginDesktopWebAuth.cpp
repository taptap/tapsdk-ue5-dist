// Copyright TapTap. All Rights Reserved.

#include "TapLoginDesktopWebAuth.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "GenericPlatform/GenericPlatformHttp.h"

FString FTapLoginDesktopWebAuth::BuildAuthorizationURL(
	const FString& BaseURL,
	const FString& ClientID,
	const FString& RedirectURI,
	const FString& State,
	const FString& CodeChallenge,
	const FString& Scope)
{
	return FString::Printf(
		TEXT("%s?client_id=%s&response_type=code&redirect_uri=%s&state=%s&code_challenge=%s&code_challenge_method=S256&scope=%s"),
		*BaseURL,
		*FGenericPlatformHttp::UrlEncode(ClientID),
		*FGenericPlatformHttp::UrlEncode(RedirectURI),
		*FGenericPlatformHttp::UrlEncode(State),
		*FGenericPlatformHttp::UrlEncode(CodeChallenge),
		*FGenericPlatformHttp::UrlEncode(Scope));
}

bool FTapLoginDesktopWebAuth::TryParseCallbackPath(
	const FString& Path,
	const FString& ExpectedState,
	FString& OutCode)
{
	FString IgnoredError;
	return TryParseCallbackPath(Path, ExpectedState, OutCode, IgnoredError);
}

bool FTapLoginDesktopWebAuth::TryParseCallbackPath(
	const FString& Path,
	const FString& ExpectedState,
	FString& OutCode,
	FString& OutError)
{
	OutCode.Empty();
	OutError.Empty();

	FString PathOnly;
	FString QueryString;
	if (!Path.Split(TEXT("?"), &PathOnly, &QueryString) || PathOnly != TEXT("/authorize"))
	{
		return false;
	}

	int32 FragmentStart = INDEX_NONE;
	if (QueryString.FindChar(TEXT('#'), FragmentStart))
	{
		QueryString.LeftInline(FragmentStart);
	}

	TMap<FString, FString> Params;
	TArray<FString> Pairs;
	QueryString.ParseIntoArray(Pairs, TEXT("&"), true);
	for (const FString& Pair : Pairs)
	{
		FString Key;
		FString Value;
		if (Pair.Split(TEXT("="), &Key, &Value))
		{
			Params.Add(
				FGenericPlatformHttp::UrlDecode(Key),
				FGenericPlatformHttp::UrlDecode(Value));
		}
	}

	const FString State = Params.FindRef(TEXT("state"));
	if (ExpectedState.IsEmpty() || State != ExpectedState)
	{
		return false;
	}

	OutError = Params.FindRef(TEXT("error"));
	OutCode = Params.FindRef(TEXT("code"));
	if (!OutError.IsEmpty())
	{
		return true;
	}

	if (OutCode.IsEmpty())
	{
		OutCode.Empty();
		return false;
	}
	return true;
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#include "TapDeepLinkHandler.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapLoginModule.h"
#include "Async/Async.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformProcess.h"

#if PLATFORM_WINDOWS
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

FTapDeepLinkHandler& FTapDeepLinkHandler::Get()
{
	static FTapDeepLinkHandler Instance;
	return Instance;
}

FTapDeepLinkHandler::FTapDeepLinkHandler()
{
}

FTapDeepLinkHandler::~FTapDeepLinkHandler()
{
	StopListening();
}

void FTapDeepLinkHandler::StartListening(const FString& ClientID, FOnDeepLinkReceived OnReceived)
{
	StopListening();

	ListenClientID = ClientID;
	DeepLinkCallback = OnReceived;
	bListening = true;

#if PLATFORM_MAC
	UE_LOG(LogTapLogin, Log, TEXT("[DeepLink] Start listening for open-taptap-%s://"), *ClientID);
	RegisterAppleEventHandler();
#elif PLATFORM_WINDOWS
	FString URL;
	FString CommandLineClientID;
	if (ParseCommandLineDeepLink(URL, CommandLineClientID))
	{
		UE_LOG(LogTapLogin, Log, TEXT("[DeepLink Win] Processing command-line callback"));
		HandleURL(URL);
	}
#endif
}

void FTapDeepLinkHandler::StopListening()
{
	if (!bListening) return;

	bListening = false;
	DeepLinkCallback.Unbind();

#if PLATFORM_MAC
	UnregisterAppleEventHandler();
#endif

	UE_LOG(LogTapLogin, Log, TEXT("[DeepLink] Stopped listening"));
}

void FTapDeepLinkHandler::HandleURL(const FString& URL)
{
	UE_LOG(LogTapLogin, Log, TEXT("[DeepLink] Received URL"));

	if (!bListening || !DeepLinkCallback.IsBound())
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink] Not listening, ignoring URL"));
		return;
	}

	const FString ExpectedScheme = FString::Printf(TEXT("open-taptap-%s://"), *ListenClientID);
	if (!URL.StartsWith(ExpectedScheme, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink] URL scheme mismatch"));
		return;
	}

	const int32 QueryStart = URL.Find(TEXT("?"));
	if (QueryStart == INDEX_NONE)
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink] No query parameters in URL"));
		return;
	}

	const TMap<FString, FString> Params = ParseQueryParams(URL.Mid(QueryStart + 1));
	const FString Code = Params.FindRef(TEXT("code"));
	const FString State = Params.FindRef(TEXT("state"));
	const FString Error = Params.FindRef(TEXT("error"));
	if (Code.IsEmpty() && Error.IsEmpty())
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink] No code or error parameter in URL"));
		return;
	}

	if (IsInGameThread())
	{
		DeepLinkCallback.ExecuteIfBound(Code, State, Error);
	}
	else
	{
		AsyncTask(ENamedThreads::GameThread, [this, Code, State, Error]()
		{
			DeepLinkCallback.ExecuteIfBound(Code, State, Error);
		});
	}
}

TMap<FString, FString> FTapDeepLinkHandler::ParseQueryParams(const FString& QueryString)
{
	TMap<FString, FString> Result;
	TArray<FString> Pairs;
	QueryString.ParseIntoArray(Pairs, TEXT("&"), true);

	for (const FString& Pair : Pairs)
	{
		FString Key, Value;
		if (Pair.Split(TEXT("="), &Key, &Value))
		{
			Result.Add(FGenericPlatformHttp::UrlDecode(Key), FGenericPlatformHttp::UrlDecode(Value));
		}
	}
	return Result;
}

#if PLATFORM_WINDOWS
bool FTapDeepLinkHandler::TryParseWindowsProtocolURL(const FString& URL, FString& OutClientID)
{
	OutClientID.Empty();
	constexpr TCHAR SchemePrefix[] = TEXT("open-taptap-");
	if (!URL.StartsWith(SchemePrefix, ESearchCase::IgnoreCase))
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink Win] Rejected callback with invalid scheme"));
		return false;
	}

	const int32 SchemeEnd = URL.Find(TEXT("://"), ESearchCase::IgnoreCase);
	if (SchemeEnd == INDEX_NONE)
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink Win] Rejected malformed callback URL"));
		return false;
	}

	OutClientID = URL.Mid(UE_ARRAY_COUNT(SchemePrefix) - 1, SchemeEnd - (UE_ARRAY_COUNT(SchemePrefix) - 1));
	if (OutClientID.IsEmpty())
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink Win] Rejected callback without ClientID"));
		return false;
	}

	for (const TCHAR Character : OutClientID)
	{
		if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_'))
		{
			OutClientID.Empty();
			UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink Win] Rejected callback with invalid ClientID"));
			return false;
		}
	}

	const int32 QueryStart = URL.Find(TEXT("?"), ESearchCase::CaseSensitive, ESearchDir::FromStart, SchemeEnd + 3);
	if (QueryStart == INDEX_NONE || QueryStart + 1 >= URL.Len())
	{
		OutClientID.Empty();
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink Win] Rejected callback without query parameters"));
		return false;
	}

	const FString CallbackPath = URL.Mid(SchemeEnd + 3, QueryStart - (SchemeEnd + 3));
	if (!CallbackPath.Equals(TEXT("authorize"), ESearchCase::IgnoreCase)
		&& !CallbackPath.Equals(TEXT("authorize/"), ESearchCase::IgnoreCase))
	{
		OutClientID.Empty();
		UE_LOG(LogTapLogin, Warning, TEXT("[DeepLink Win] Rejected unsupported callback path"));
		return false;
	}

	return true;
}

bool FTapDeepLinkHandler::ParseCommandLineDeepLink(FString& OutURL, FString& OutClientID)
{
	OutURL.Empty();
	OutClientID.Empty();
	if (!FParse::Value(FCommandLine::Get(), TEXT("--taptap-deeplink="), OutURL))
	{
		return false;
	}

	OutURL.TrimQuotesInline();
	return TryParseWindowsProtocolURL(OutURL, OutClientID);
}

#endif

#if PLATFORM_LINUX
// Linux 暂不支持 DeepLink, 使用 localhost HTTP fallback
#endif

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

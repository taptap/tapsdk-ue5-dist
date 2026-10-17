// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttp.h"
#include "Internal/TapLogPrivacy.h"
#include "Internal/Http/TapHttpAuthorizationService.h"
#include "Internal/Http/TapHttpSign.h"
#include "Internal/Http/TapHttpParser.h"
#include "Internal/Http/TapHttpCommonParams.h"
#include "Internal/Http/TapHttpTime.h"
#include "Internal/Http/TapHttpErrorConstants.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "TapCoreModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapHttp2, Log, All);

namespace
{
    /** 按字段脱敏后打印 HTTP 请求 (受 FTapHttp::IsLogEnabled 控制). 对齐 Unity TapHttpUtils.PrintRequest. */
    void LogHttpRequest(const FString& Method, const FString& Url,
        const TMap<FString, FString>& Headers, const FString& Body)
    {
        if (!FTapHttp::IsLogEnabled()) return;

        FString Sb;
        Sb += TEXT("\n=== HTTP Request Start ===\n");
        Sb += FString::Printf(TEXT("URL: %s\n"), *TapLogPrivacy::Url(Url));
        Sb += FString::Printf(TEXT("Method: %s\n"), *Method);
        Sb += TEXT("Headers:\n");
        for (const auto& KV : Headers)
        {
            Sb += FString::Printf(TEXT("  %s: %s\n"), *KV.Key, *TapLogPrivacy::Header(KV.Key, KV.Value));
        }
        if (!Body.IsEmpty())
        {
            Sb += FString::Printf(TEXT("Content: %s\n"), *TapLogPrivacy::Body(Body));
        }
        Sb += TEXT("=== HTTP Request End ===");
        UE_LOG(LogTapHttp2, Log, TEXT("%s"), *Sb);
    }

    /** 按字段脱敏后打印 HTTP 响应 (受 FTapHttp::IsLogEnabled 控制). 对齐 Unity TapHttpUtils.PrintResponse. */
    void LogHttpResponse(const FString& Url, int32 StatusCode, const FString& Content)
    {
        if (!FTapHttp::IsLogEnabled()) return;

        FString Sb;
        Sb += TEXT("\n=== HTTP Response Start ===\n");
        Sb += FString::Printf(TEXT("URL: %s\n"), *TapLogPrivacy::Url(Url));
        Sb += FString::Printf(TEXT("Status Code: %d\n"), StatusCode);
        if (!Content.IsEmpty())
        {
            Sb += FString::Printf(TEXT("Content: %s\n"), *TapLogPrivacy::Body(Content));
        }
        Sb += TEXT("=== HTTP Response End ===");
        UE_LOG(LogTapHttp2, Log, TEXT("%s"), *Sb);
    }
}

// 静态常量初始化
const FString FTapHttp::HOST_CN = TEXT("https://tapsdk.tapapis.cn");
const FString FTapHttp::HOST_IO = TEXT("https://tapsdk.tapapis.com");
const FString FTapHttp::HOST_RND_CN = TEXT("https://tapsdk.api.xdrnd.cn");
const FString FTapHttp::HOST_RND_IO = TEXT("https://tapsdk.api.xdrnd.com");

// 静态成员初始化
bool FTapHttp::bIsInitialized = false;
ETapRegion FTapHttp::CurrentRegion = ETapRegion::CN;
bool FTapHttp::bEnableLog = false;
bool FTapHttp::bIsRND = false;
TSharedPtr<ITapHttpAuthorizationService> FTapHttp::GlobalAuthorizationService = nullptr;

// ============== FTapHttp 初始化 ==============

void FTapHttp::Init(
	const FString& ClientID,
	const FString& ClientToken,
	ETapRegion Region,
	bool bInEnableLog,
	bool bInIsRND)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTapHttp2, Warning, TEXT("FTapHttp already initialized, re-initializing with new config"));
	}

	// 存储全局配置
	CurrentRegion = Region;
	bEnableLog = bInEnableLog;
	bIsRND = bInIsRND;

	// 初始化公共参数管理器
	FTapHttpCommonParams::Initialize(ClientID, ClientToken);

	bIsInitialized = true;

	UE_LOG(LogTapHttp2, Log, TEXT("FTapHttp initialized: Region=%s, ClientID=%s, EnableLog=%s, RND=%s"),
		CurrentRegion == ETapRegion::CN ? TEXT("CN") : TEXT("Global"),
		*ClientID,
		bEnableLog ? TEXT("true") : TEXT("false"),
		bIsRND ? TEXT("true") : TEXT("false"));
}

bool FTapHttp::IsInitialized()
{
	return bIsInitialized;
}

ETapRegion FTapHttp::GetRegion()
{
	return CurrentRegion;
}

bool FTapHttp::IsLogEnabled()
{
	return bEnableLog;
}

void FTapHttp::SetAuthorizationService(TSharedPtr<ITapHttpAuthorizationService> Service)
{
	GlobalAuthorizationService = Service;
}

TSharedPtr<ITapHttpAuthorizationService> FTapHttp::GetAuthorizationService()
{
	return GlobalAuthorizationService;
}

// ============== FTapHttp 实现 ==============

TSharedPtr<FTapHttpBuilder> FTapHttp::NewBuilder(const FString& ModuleName, const FString& ModuleVersion)
{
	return MakeShared<FTapHttpBuilder>(ModuleName, ModuleVersion);
}

FTapHttp::FTapHttp(const FTapHttpConfig& InConfig)
	: Config(InConfig)
{
	UE_LOG(LogTapHttp2, Log, TEXT("FTapHttp created for module: %s v%s"),
		*Config.ModuleName, *Config.ModuleVersion);
}

void FTapHttp::Get(
	const FString& Path,
	const TMap<FString, FString>& QueryParams,
	bool bEnableAuthorization,
	TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	// GET 请求默认使用 Fixed(3) 重试策略
	if (!RetryStrategy.IsValid())
	{
		RetryStrategy = FTapHttpRetryStrategy::CreateDefault(
			FTapHttpBackoffStrategy::CreateFixed(DEFAULT_GET_RETRY_COUNT)
		);
	}

	FString Url = BuildUrl(Path, QueryParams);
	TMap<FString, FString> Headers;

	ExecuteRequest(
		ETapHttpMethod::Get,
		Url,
		Headers,
		TEXT(""),
		bEnableAuthorization,
		RetryStrategy,
		OnComplete
	);
}

void FTapHttp::Get(
	const FString& Path,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	Get(Path, TMap<FString, FString>(), false, nullptr, OnComplete);
}

void FTapHttp::PostJson(
	const FString& Path,
	const TMap<FString, FString>& QueryParams,
	const TSharedPtr<FJsonObject>& JsonBody,
	bool bEnableAuthorization,
	TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	// POST 请求默认不重试
	if (!RetryStrategy.IsValid())
	{
		RetryStrategy = FTapHttpRetryStrategy::CreateNone();
	}

	FString Url = BuildUrl(Path, QueryParams);
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/json"));

	// 序列化 JSON
	FString Body;
	if (JsonBody.IsValid())
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		FJsonSerializer::Serialize(JsonBody.ToSharedRef(), Writer);
	}

	ExecuteRequest(
		ETapHttpMethod::Post,
		Url,
		Headers,
		Body,
		bEnableAuthorization,
		RetryStrategy,
		OnComplete
	);
}

void FTapHttp::PostJson(
	const FString& Path,
	const TSharedPtr<FJsonObject>& JsonBody,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	PostJson(Path, TMap<FString, FString>(), JsonBody, false, nullptr, OnComplete);
}

void FTapHttp::PostForm(
	const FString& Path,
	const TMap<FString, FString>& QueryParams,
	const TMap<FString, FString>& FormData,
	bool bEnableAuthorization,
	TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	// POST 请求默认不重试
	if (!RetryStrategy.IsValid())
	{
		RetryStrategy = FTapHttpRetryStrategy::CreateNone();
	}

	FString Url = BuildUrl(Path, QueryParams);
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));

	// 构建表单数据
	FString Body;
	bool bFirst = true;
	for (const auto& Pair : FormData)
	{
		if (!bFirst)
		{
			Body += TEXT("&");
		}
		Body += FString::Printf(TEXT("%s=%s"),
			*FGenericPlatformHttp::UrlEncode(Pair.Key),
			*FGenericPlatformHttp::UrlEncode(Pair.Value));
		bFirst = false;
	}

	ExecuteRequest(
		ETapHttpMethod::Post,
		Url,
		Headers,
		Body,
		bEnableAuthorization,
		RetryStrategy,
		OnComplete
	);
}

void FTapHttp::PostForm(
	const FString& Path,
	const TMap<FString, FString>& FormData,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	PostForm(Path, TMap<FString, FString>(), FormData, false, nullptr, OnComplete);
}

void FTapHttp::Get(
	const FString& Path,
	const TMap<FString, FString>& QueryParams,
	const TMap<FString, FString>& ExtraHeaders,
	bool bEnableAuthorization,
	TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	// GET 请求默认使用 Fixed(3) 重试策略
	if (!RetryStrategy.IsValid())
	{
		RetryStrategy = FTapHttpRetryStrategy::CreateDefault(
			FTapHttpBackoffStrategy::CreateFixed(DEFAULT_GET_RETRY_COUNT)
		);
	}

	const FString Url = BuildUrl(Path, QueryParams);
	ExecuteRequest(ETapHttpMethod::Get, Url, ExtraHeaders, FString(),
		bEnableAuthorization, RetryStrategy, OnComplete);
}

void FTapHttp::PostJson(
	const FString& Path,
	const TMap<FString, FString>& QueryParams,
	const TMap<FString, FString>& ExtraHeaders,
	const TSharedPtr<FJsonObject>& JsonBody,
	bool bEnableAuthorization,
	TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	// POST 请求默认不重试
	if (!RetryStrategy.IsValid())
	{
		RetryStrategy = FTapHttpRetryStrategy::CreateNone();
	}

	const FString Url = BuildUrl(Path, QueryParams);

	// 合并 ExtraHeaders 并设置默认 Content-Type
	TMap<FString, FString> Headers = ExtraHeaders;
	if (!Headers.Contains(TEXT("Content-Type")))
	{
		Headers.Add(TEXT("Content-Type"), TEXT("application/json; charset=utf-8"));
	}

	// 序列化 JSON body
	FString Body;
	if (JsonBody.IsValid())
	{
		TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
		FJsonSerializer::Serialize(JsonBody.ToSharedRef(), Writer);
	}

	ExecuteRequest(ETapHttpMethod::Post, Url, Headers, Body,
		bEnableAuthorization, RetryStrategy, OnComplete);
}

void FTapHttp::PostForm(
	const FString& Path,
	const TMap<FString, FString>& QueryParams,
	const TMap<FString, FString>& ExtraHeaders,
	const TMap<FString, FString>& FormData,
	bool bEnableAuthorization,
	TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
	const FTapHttpJsonResultDelegate& OnComplete)
{
	// POST 请求默认不重试
	if (!RetryStrategy.IsValid())
	{
		RetryStrategy = FTapHttpRetryStrategy::CreateNone();
	}

	const FString Url = BuildUrl(Path, QueryParams);

	// 合并 ExtraHeaders 并设置默认 Content-Type
	TMap<FString, FString> Headers = ExtraHeaders;
	if (!Headers.Contains(TEXT("Content-Type")))
	{
		Headers.Add(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
	}

	// 构建 form-urlencoded body
	FString Body;
	bool bFirst = true;
	for (const auto& Pair : FormData)
	{
		if (!bFirst)
		{
			Body += TEXT("&");
		}
		Body += FString::Printf(TEXT("%s=%s"),
			*FGenericPlatformHttp::UrlEncode(Pair.Key),
			*FGenericPlatformHttp::UrlEncode(Pair.Value));
		bFirst = false;
	}

	ExecuteRequest(ETapHttpMethod::Post, Url, Headers, Body,
		bEnableAuthorization, RetryStrategy, OnComplete);
}

FString FTapHttp::GetHost() const
{
	if (!Config.Domain.IsEmpty())
	{
		return Config.Domain;
	}

	// 根据 RND 标志和区域选择域名
	if (bIsRND)
	{
		return CurrentRegion == ETapRegion::CN ? HOST_RND_CN : HOST_RND_IO;
	}
	return CurrentRegion == ETapRegion::CN ? HOST_CN : HOST_IO;
}

FString FTapHttp::BuildUrl(const FString& Path, const TMap<FString, FString>& QueryParams) const
{
	FString Host = GetHost();
	FString Url;

	// 如果 Path 已经是完整 URL，直接使用
	if (Path.StartsWith(TEXT("http://")) || Path.StartsWith(TEXT("https://")))
	{
		Url = Path;
	}
	else
	{
		// 确保 Path 以 / 开头
		FString NormalizedPath = Path;
		if (!NormalizedPath.StartsWith(TEXT("/")))
		{
			NormalizedPath = TEXT("/") + NormalizedPath;
		}
		Url = Host + NormalizedPath;
	}

	// 添加公共查询参数
	TMap<FString, FString> AllQueryParams = QueryParams;
	if (Config.bEnableCommonParams)
	{
		TMap<FString, FString> CommonParams = FTapHttpCommonParams::GetCommonQueryParams();
		for (const auto& Param : CommonParams)
		{
			if (!AllQueryParams.Contains(Param.Key))
			{
				AllQueryParams.Add(Param.Key, Param.Value);
			}
		}
	}

	// 拼接查询参数
	if (AllQueryParams.Num() > 0)
	{
		FString Separator = Url.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");
		for (const auto& Param : AllQueryParams)
		{
			Url += FString::Printf(TEXT("%s%s=%s"),
				*Separator,
				*FGenericPlatformHttp::UrlEncode(Param.Key),
				*FGenericPlatformHttp::UrlEncode(Param.Value));
			Separator = TEXT("&");
		}
	}

	return Url;
}

void FTapHttp::ExecuteRequest(
	ETapHttpMethod Method,
	const FString& Url,
	const TMap<FString, FString>& Headers,
	const FString& Body,
	bool bEnableAuthorization,
	TSharedPtr<ITapHttpRetryStrategy> RetryStrategy,
	const FTapHttpJsonResultDelegate& OnComplete,
	int32 RetryCount)
{
	UE_LOG(LogTapHttp2, Log, TEXT("Executing %s request to: %s (retry: %d)"),
		*TapHttpMethodHelper::ToString(Method), *TapLogPrivacy::Url(Url), RetryCount);

	// 创建请求
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(TapHttpMethodHelper::ToString(Method));
	HttpRequest->SetURL(Url);
	HttpRequest->SetTimeout(Config.ConnectTimeout);

	// 合并请求头
	TMap<FString, FString> AllHeaders = Headers;

	// 添加公共请求头
	if (Config.bEnableCommonParams && Config.Sign.IsValid())
	{
		TMap<FString, FString> CommonHeaders = Config.Sign->GetFixedHeaders(
			Url, TapHttpMethodHelper::ToString(Method),
			Config.ModuleName, Config.ModuleVersion, bEnableAuthorization);
		for (const auto& Header : CommonHeaders)
		{
			if (!AllHeaders.Contains(Header.Key))
			{
				AllHeaders.Add(Header.Key, Header.Value);
			}
		}
	}

	// Authorization (MAC) - 对齐 Android DefaultCommonArgsInterceptor.getAuthorization
	if (bEnableAuthorization)
	{
		TSharedPtr<ITapHttpAuthorizationService> AuthService =
			Config.AuthorizationService.IsValid()
				? Config.AuthorizationService
				: FTapHttp::GetAuthorizationService();
		if (AuthService.IsValid())
		{
			FString Auth = AuthService->ObtainAuthorization(Url, TapHttpMethodHelper::ToString(Method));
			if (!Auth.IsEmpty())
			{
				AllHeaders.Add(TEXT("Authorization"), Auth);
			}
		}
		else
		{
			UE_LOG(LogTapHttp2, Warning,
				TEXT("[FTapHttp] bEnableAuthorization=true but no AuthorizationService registered, skipping Authorization header"));
		}
	}

	// 添加签名
	if (Config.bEnableSigning && Config.bEnableCommonParams && Config.Sign.IsValid())
	{
		FTapHttpSignData SignData{
			Url, TapHttpMethodHelper::ToString(Method),
			bEnableAuthorization, Body, AllHeaders
		};
		Config.Sign->Sign(SignData);
	}

	// 设置请求头
	for (const auto& Header : AllHeaders)
	{
		HttpRequest->SetHeader(Header.Key, Header.Value);
	}

	// 设置请求体
	if (!Body.IsEmpty())
	{
		HttpRequest->SetContentAsString(Body);
		UE_LOG(LogTapHttp2, Verbose, TEXT("Request body: %s"), *TapLogPrivacy::Body(Body));
	}

	// 请求日志（敏感字段脱敏） (受 IsLogEnabled 控制)
	LogHttpRequest(TapHttpMethodHelper::ToString(Method), Url, AllHeaders, Body);

	// 设置完成回调
	// 使用弱引用避免循环引用
	TWeakPtr<FTapHttp> SelfWeakPtr = AsShared();

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[SelfWeakPtr, RetryStrategy, OnComplete, Method, Url, Headers, Body, bEnableAuthorization, RetryCount]
		(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			TSharedPtr<FTapHttp> StrongThis = SelfWeakPtr.Pin();
			if (!StrongThis.IsValid())
			{
				UE_LOG(LogTapHttp2, Warning, TEXT("FTapHttp instance destroyed before callback"));
				return;
			}

			int32 StatusCode = 0;
			FString Content;

			if (Response.IsValid())
			{
				StatusCode = Response->GetResponseCode();
				Content = Response->GetContentAsString();
				UE_LOG(LogTapHttp2, Log, TEXT("Response: status=%d, length=%d"), StatusCode, Content.Len());
				UE_LOG(LogTapHttp2, Verbose, TEXT("Response content: %s"), *TapLogPrivacy::Body(Content));
			}
			else
			{
				UE_LOG(LogTapHttp2, Warning, TEXT("No response received"));
			}

			// 响应日志（敏感字段脱敏） (受 IsLogEnabled 控制)
			LogHttpResponse(Url, StatusCode, Content);

			// 解析响应
			FTapHttpJsonResult Result;
			if (StrongThis->Config.Parser.IsValid())
			{
				Result = StrongThis->Config.Parser->Parse(StatusCode, Content, bWasSuccessful, TEXT(""));
			}
			else
			{
				Result = FTapHttpJsonResult::NetworkError(TEXT("Parser not configured"));
			}

			// 检查是否需要重试
			if (!Result.IsSuccess() && RetryStrategy.IsValid())
			{
				int64 RetryMillis = RetryStrategy->NextRetryMillis(Result.GetException());
				if (RetryMillis >= 0)
				{
					UE_LOG(LogTapHttp2, Log, TEXT("Will retry after %lld ms"), RetryMillis);

					// 延迟重试
					if (RetryMillis > 0)
					{
						FTSTicker::GetCoreTicker().AddTicker(
							FTickerDelegate::CreateLambda(
								[SelfWeakPtr, Method, Url, Headers, Body, bEnableAuthorization, RetryStrategy, OnComplete, RetryCount](float DeltaTime) -> bool
								{
									TSharedPtr<FTapHttp> RetryThis = SelfWeakPtr.Pin();
									if (RetryThis.IsValid())
									{
										RetryThis->ExecuteRequest(
											Method, Url, Headers, Body,
											bEnableAuthorization, RetryStrategy,
											OnComplete, RetryCount + 1
										);
									}
									return false; // 只执行一次
								}
							),
							RetryMillis / 1000.0f
						);
					}
					else
					{
						// 立即重试
						StrongThis->ExecuteRequest(
							Method, Url, Headers, Body,
							bEnableAuthorization, RetryStrategy,
							OnComplete, RetryCount + 1
						);
					}
					return;
				}
			}

			// 执行回调
			OnComplete.ExecuteIfBound(Result);
		}
	);

	// 发送请求
	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTapHttp2, Error, TEXT("Failed to start HTTP request"));
		FTapHttpJsonResult Result = FTapHttpJsonResult::NetworkError(TEXT("Failed to start HTTP request"));
		OnComplete.ExecuteIfBound(Result);
	}
}

// ============== FTapHttpBuilder 实现 ==============

FTapHttpBuilder::FTapHttpBuilder(const FString& ModuleName, const FString& ModuleVersion)
{
	Config.ModuleName = ModuleName;
	Config.ModuleVersion = ModuleVersion;
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::Domain(const FString& InDomain)
{
	Config.Domain = InDomain;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::ConnectTimeout(float Seconds)
{
	Config.ConnectTimeout = Seconds;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::ReadTimeout(float Seconds)
{
	Config.ReadTimeout = Seconds;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::WriteTimeout(float Seconds)
{
	Config.WriteTimeout = Seconds;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::EnableSigning(bool bEnable)
{
	Config.bEnableSigning = bEnable;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::EnableCommonParams(bool bEnable)
{
	Config.bEnableCommonParams = bEnable;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::Sign(TSharedRef<ITapHttpSign> InSign)
{
	Config.Sign = InSign;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::Parser(TSharedRef<ITapHttpParser> InParser)
{
	Config.Parser = InParser;
	return AsShared();
}

TSharedPtr<FTapHttpBuilder> FTapHttpBuilder::Authorization(TSharedRef<ITapHttpAuthorizationService> InService)
{
	Config.AuthorizationService = InService;
	return AsShared();
}

TSharedPtr<FTapHttp> FTapHttpBuilder::Build()
{
	if (!Config.Sign.IsValid())
	{
		Config.Sign = FTapHttpSign::CreateDefaultSign();
	}
	if (!Config.Parser.IsValid())
	{
		Config.Parser = FTapHttpParser::CreateEnvelopeParser();
	}
	return MakeShared<FTapHttp>(Config);
}

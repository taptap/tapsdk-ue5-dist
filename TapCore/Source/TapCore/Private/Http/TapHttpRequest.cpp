// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttpRequest.h"
#include "Internal/TapLogPrivacy.h"
#include "Internal/Http/TapHttpCommonParams.h"
#include "Internal/Http/TapHttpErrorConstants.h"
#include "Internal/Http/TapHttpTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapHttp, Log, All);

TMap<FString, FString> FTapHttpRequest::DefaultHeaders;
bool FTapHttpRequest::bEnableCommonParams = false;
bool FTapHttpRequest::bEnableSigning = false;
FString FTapHttpRequest::ModuleName = TEXT("TapCore");
FString FTapHttpRequest::ModuleVersion = TEXT("1.0.0");

void FTapHttpRequest::Get(
	const FString& URL,
	const FTapHttpRequestCompleteDelegate& OnComplete,
	float Timeout)
{
	TMap<FString, FString> Headers;
	Request(TEXT("GET"), URL, Headers, TEXT(""), OnComplete, Timeout);
}

void FTapHttpRequest::Post(
	const FString& URL,
	const FString& JsonBody,
	const FTapHttpRequestCompleteDelegate& OnComplete,
	float Timeout)
{
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
	Request(TEXT("POST"), URL, Headers, JsonBody, OnComplete, Timeout);
}

void FTapHttpRequest::PostJson(
	const FString& URL,
	const TSharedPtr<FJsonObject>& JsonObject,
	const FTapHttpRequestCompleteDelegate& OnComplete,
	float Timeout)
{
	if (!JsonObject.IsValid())
	{
		UE_LOG(LogTapHttp, Error, TEXT("PostJson: Invalid JSON object"));
		FTapHttpResponse Result;
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Invalid JSON object");
		OnComplete.ExecuteIfBound(Result);
		return;
	}

	// Convert JSON object to string
	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer))
	{
		Post(URL, OutputString, OnComplete, Timeout);
	}
	else
	{
		UE_LOG(LogTapHttp, Error, TEXT("PostJson: Failed to serialize JSON object"));
		FTapHttpResponse Result;
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to serialize JSON object");
		OnComplete.ExecuteIfBound(Result);
	}
}

void FTapHttpRequest::Put(
	const FString& URL,
	const FString& JsonBody,
	const FTapHttpRequestCompleteDelegate& OnComplete,
	float Timeout)
{
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/json"));
	Request(TEXT("PUT"), URL, Headers, JsonBody, OnComplete, Timeout);
}

void FTapHttpRequest::Delete(
	const FString& URL,
	const FTapHttpRequestCompleteDelegate& OnComplete,
	float Timeout)
{
	TMap<FString, FString> Headers;
	Request(TEXT("DELETE"), URL, Headers, TEXT(""), OnComplete, Timeout);
}

void FTapHttpRequest::Request(
	const FString& Method,
	const FString& URL,
	const TMap<FString, FString>& Headers,
	const FString& Body,
	const FTapHttpRequestCompleteDelegate& OnComplete,
	float Timeout)
{
	UE_LOG(LogTapHttp, Log, TEXT("HTTP %s: %s"), *Method, *TapLogPrivacy::Url(URL));

	// Prepare final URL with query params
	FString FinalURL = URL;

	// Add common query parameters if enabled
	if (bEnableCommonParams)
	{
		TMap<FString, FString> CommonQueryParams = FTapHttpCommonParams::GetCommonQueryParams();
		if (CommonQueryParams.Num() > 0)
		{
			FString Separator = URL.Contains(TEXT("?")) ? TEXT("&") : TEXT("?");
			for (const auto& Param : CommonQueryParams)
			{
				FinalURL += FString::Printf(TEXT("%s%s=%s"), *Separator, *Param.Key, *Param.Value);
				Separator = TEXT("&");
			}
			UE_LOG(LogTapHttp, Verbose, TEXT("Added common query params, final URL: %s"), *TapLogPrivacy::Url(FinalURL));
		}
	}

	// Create HTTP request
	TSharedRef<IHttpRequest> HttpRequest = CreateRequest(Method, FinalURL, Timeout);

	// Merge all headers
	TMap<FString, FString> AllHeaders;

	// 1. Add common headers if enabled
	if (bEnableCommonParams)
	{
		TMap<FString, FString> CommonHeaders = FTapHttpCommonParams::GetCommonHeaders(
			ModuleName,
			ModuleVersion,
			false // Authorization handled separately
		);
		for (const auto& Header : CommonHeaders)
		{
			AllHeaders.Add(Header.Key, Header.Value);
		}
		UE_LOG(LogTapHttp, Verbose, TEXT("Added %d common headers"), CommonHeaders.Num());
	}

	// 2. Add default headers (can override common headers)
	for (const auto& Header : DefaultHeaders)
	{
		AllHeaders.Add(Header.Key, Header.Value);
	}

	// 3. Add custom headers (can override all previous headers)
	for (const auto& Header : Headers)
	{
		AllHeaders.Add(Header.Key, Header.Value);
	}

	// Add signature if enabled
	if (bEnableSigning && bEnableCommonParams)
	{
		FTapHttpCommonParams::AddSignature(Method, FinalURL, AllHeaders, Body);
		UE_LOG(LogTapHttp, Verbose, TEXT("Added request signature"));
	}

	// Set all headers to request
	for (const auto& Header : AllHeaders)
	{
		HttpRequest->SetHeader(Header.Key, Header.Value);
	}

	// Set body if provided
	if (!Body.IsEmpty())
	{
		HttpRequest->SetContentAsString(Body);
		UE_LOG(LogTapHttp, Verbose, TEXT("Request Body: %s"), *TapLogPrivacy::Body(Body));
	}

	// Set completion callback
	HttpRequest->OnProcessRequestComplete().BindLambda(
		[OnComplete](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
		{
			ProcessResponse(Request, Response, bWasSuccessful, OnComplete);
		}
	);

	// Send request
	UE_LOG(LogTapHttp, Log, TEXT("Calling ProcessRequest() for URL: %s"), *TapLogPrivacy::Url(FinalURL));
	bool bRequestStarted = HttpRequest->ProcessRequest();
	UE_LOG(LogTapHttp, Log, TEXT("ProcessRequest() returned: %s"), bRequestStarted ? TEXT("true") : TEXT("false"));

	if (!bRequestStarted)
	{
		UE_LOG(LogTapHttp, Error, TEXT("Failed to start HTTP request"));
		FTapHttpResponse Result;
		Result.bSuccess = false;
		Result.ErrorMessage = TEXT("Failed to start HTTP request");
		OnComplete.ExecuteIfBound(Result);
	}
}

// ---------------------------------------------------------------------------
// RequestWithSigning: 每个 attempt 重新构建 X-Tap-* + 重签, 对齐 Android TapHttpCall
// ---------------------------------------------------------------------------
namespace
{
	/**
	 * 真正发包. 把整个请求 (method/url/body/extra headers/module info) 装进闭包,
	 * 失败 + invalid_time + 允许重试时, 递归调自己 (AttemptIndex+1) 重新构建 header.
	 */
	void SendWithSigningOnce(
		const FString& Method,
		const FString& URL,
		const TMap<FString, FString>& ExtraHeaders,
		const FString& Body,
		const FString& ModuleName,
		const FString& ModuleVersion,
		bool bEnableAuthorization,
		bool bEnableInvalidTimeRetry,
		int32 AttemptIndex,
		const FTapHttpRequestCompleteDelegate& OnComplete,
		float Timeout)
	{
		// 1. 每次都从 FTapHttpCommonParams::GetCommonHeaders 拿公共头:
		//    X-Tap-Ts 内部用 FTapHttpTime::GetCurrentTime(), FixTime 校准后会自动用校准值.
		TMap<FString, FString> AllHeaders = FTapHttpCommonParams::GetCommonHeaders(
			ModuleName, ModuleVersion, bEnableAuthorization);

		// 2. 调用方的额外 header (Content-Type, MAC Authorization, anti-addiction token...) 覆盖
		for (const TPair<FString, FString>& KV : ExtraHeaders)
		{
			AllHeaders.Add(KV.Key, KV.Value);
		}

		// 3. 用最终的 header 集合 + body 重新签 X-Tap-Sign (依赖 X-Tap-Ts, 必须放在最后)
		FTapHttpCommonParams::AddSignature(Method, URL, AllHeaders, Body);

		// 4. 创建并发送 HTTP 请求
		TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
		HttpRequest->SetVerb(Method);
		HttpRequest->SetURL(URL);
		HttpRequest->SetTimeout(Timeout);
		HttpRequest->SetHeader(TEXT("User-Agent"), TEXT("TapSDK-UE5/1.0.0"));
		HttpRequest->SetHeader(TEXT("Accept"),     TEXT("application/json"));
		for (const TPair<FString, FString>& KV : AllHeaders)
		{
			HttpRequest->SetHeader(KV.Key, KV.Value);
		}
		if (!Body.IsEmpty())
		{
			HttpRequest->SetContentAsString(Body);
		}

		UE_LOG(LogTapHttp, Log, TEXT("HTTP %s (signed attempt=%d): %s"),
			*Method, AttemptIndex, *TapLogPrivacy::Url(URL));

		for (const TPair<FString, FString>& KV : AllHeaders)
		{
			UE_LOG(LogTapHttp, Warning, TEXT("  [sent header] %s: %s"),
				*KV.Key, *TapLogPrivacy::Header(KV.Key, KV.Value));
		}

		HttpRequest->OnProcessRequestComplete().BindLambda(
			[Method, URL, ExtraHeaders, Body, ModuleName, ModuleVersion,
			 bEnableAuthorization, bEnableInvalidTimeRetry, AttemptIndex, OnComplete, Timeout]
			(FHttpRequestPtr Req, FHttpResponsePtr Response, bool bWasSuccessful)
			{
				// 先把响应解析成 FTapHttpResponse (复用 ProcessResponse 的解析 + FixTime 副作用)
				FTapHttpResponse Result;
				Result.bSuccess = bWasSuccessful;

				if (Response.IsValid())
				{
					Result.StatusCode = Response->GetResponseCode();
					Result.Content    = Response->GetContentAsString();

					if (!Result.Content.IsEmpty())
					{
						TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result.Content);
						FJsonSerializer::Deserialize(Reader, Result.JsonObject);
					}

					// 每次响应都用 server now 校准时钟 (即使本次成功) — 对齐 Android
					int64 ServerTime = 0;
					if (Result.JsonObject.IsValid()
						&& Result.JsonObject->TryGetNumberField(TEXT("now"), ServerTime)
						&& ServerTime > 0)
					{
						FTapHttpTime::FixTime(ServerTime);
					}
				}
				else
				{
					Result.ErrorMessage = TEXT("Request failed - no response");
				}

				// 5. 判断是否需要 invalid_time 重试 (只重一次)
				if (bEnableInvalidTimeRetry
					&& AttemptIndex == 0
					&& Result.StatusCode == 400
					&& Result.JsonObject.IsValid())
				{
					// envelope.error 可能在顶层, 也可能在 data 子对象里
					FString ErrorField;
					Result.JsonObject->TryGetStringField(TEXT("error"), ErrorField);
					if (ErrorField.IsEmpty())
					{
						const TSharedPtr<FJsonObject>* DataObj = nullptr;
						if (Result.JsonObject->TryGetObjectField(TEXT("data"), DataObj) && DataObj && (*DataObj).IsValid())
						{
							(*DataObj)->TryGetStringField(TEXT("error"), ErrorField);
						}
					}

					if (ErrorField.Equals(FTapHttpErrorConstants::ERROR_INVALID_TIME))
					{
						UE_LOG(LogTapHttp, Log,
							TEXT("HTTP 400 invalid_time → time calibrated, retry once: %s"), *TapLogPrivacy::Url(URL));
						SendWithSigningOnce(
							Method, URL, ExtraHeaders, Body,
							ModuleName, ModuleVersion, bEnableAuthorization,
							bEnableInvalidTimeRetry, AttemptIndex + 1, OnComplete, Timeout);
						return;
					}
				}

				if (!Result.IsSuccess() && Result.ErrorMessage.IsEmpty())
				{
					Result.ErrorMessage = FString::Printf(TEXT("HTTP %d: %s"),
						Result.StatusCode, *Result.Content);
				}

				OnComplete.ExecuteIfBound(Result);
			}
		);

		if (!HttpRequest->ProcessRequest())
		{
			UE_LOG(LogTapHttp, Error, TEXT("Failed to start signed HTTP request: %s"), *TapLogPrivacy::Url(URL));
			FTapHttpResponse Result;
			Result.bSuccess     = false;
			Result.ErrorMessage = TEXT("Failed to start HTTP request");
			OnComplete.ExecuteIfBound(Result);
		}
	}
}

void FTapHttpRequest::RequestWithSigning(
	const FString& Method,
	const FString& URL,
	const TMap<FString, FString>& ExtraHeaders,
	const FString& Body,
	const FString& InModuleName,
	const FString& InModuleVersion,
	bool bEnableAuthorization,
	const FTapHttpRequestCompleteDelegate& OnComplete,
	bool bEnableInvalidTimeRetry,
	float Timeout)
{
	SendWithSigningOnce(
		Method, URL, ExtraHeaders, Body,
		InModuleName, InModuleVersion, bEnableAuthorization,
		bEnableInvalidTimeRetry, /*AttemptIndex=*/0,
		OnComplete, Timeout);
}

void FTapHttpRequest::SetDefaultHeaders(const TMap<FString, FString>& Headers)
{
	DefaultHeaders = Headers;
	UE_LOG(LogTapHttp, Log, TEXT("Set %d default headers"), DefaultHeaders.Num());
}

void FTapHttpRequest::AddDefaultHeader(const FString& Key, const FString& Value)
{
	DefaultHeaders.Add(Key, Value);
	UE_LOG(LogTapHttp, Verbose, TEXT("Added default header: %s = %s"), *Key, *TapLogPrivacy::Header(Key, Value));
}

void FTapHttpRequest::ClearDefaultHeaders()
{
	DefaultHeaders.Empty();
	UE_LOG(LogTapHttp, Log, TEXT("Cleared all default headers"));
}

void FTapHttpRequest::SetEnableCommonParams(bool bEnable)
{
	bEnableCommonParams = bEnable;
	UE_LOG(LogTapHttp, Log, TEXT("Common parameters %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
}

void FTapHttpRequest::SetModuleInfo(const FString& InModuleName, const FString& InModuleVersion)
{
	ModuleName = InModuleName;
	ModuleVersion = InModuleVersion;
	UE_LOG(LogTapHttp, Log, TEXT("Set module info: %s v%s"), *ModuleName, *ModuleVersion);
}

void FTapHttpRequest::SetEnableSigning(bool bEnable)
{
	bEnableSigning = bEnable;
	UE_LOG(LogTapHttp, Log, TEXT("Request signing %s"), bEnable ? TEXT("enabled") : TEXT("disabled"));
}

void FTapHttpRequest::ProcessResponse(
	FHttpRequestPtr Request,
	FHttpResponsePtr Response,
	bool bWasSuccessful,
	const FTapHttpRequestCompleteDelegate& OnComplete)
{
	UE_LOG(LogTapHttp, Log, TEXT("ProcessResponse called: bWasSuccessful=%s, Response.IsValid()=%s"),
		bWasSuccessful ? TEXT("true") : TEXT("false"),
		Response.IsValid() ? TEXT("true") : TEXT("false"));

	FTapHttpResponse Result;
	Result.bSuccess = bWasSuccessful;

	// Always try to get response data if response is valid, even if bWasSuccessful is false
	if (Response.IsValid())
	{
		Result.StatusCode = Response->GetResponseCode();
		Result.Content = Response->GetContentAsString();

		UE_LOG(LogTapHttp, Log, TEXT("HTTP Response: bWasSuccessful=%s, Status=%d, ContentLength=%d"),
			bWasSuccessful ? TEXT("true") : TEXT("false"),
			Result.StatusCode, Result.Content.Len());
		UE_LOG(LogTapHttp, Verbose, TEXT("Response Content: %s"), *TapLogPrivacy::Body(Result.Content));
	}
	else
	{
		// No response object - real network failure
		FString URL = Request.IsValid() ? Request->GetURL() : TEXT("Unknown");
		FString Status = Request.IsValid() ? FString::Printf(TEXT("Status: %d"), (int32)Request->GetStatus()) : TEXT("No Status");

		Result.ErrorMessage = FString::Printf(TEXT("Request failed - no response - URL: %s, %s"),
			*URL, *Status);

		UE_LOG(LogTapHttp, Error, TEXT("HTTP Request Failed: Request failed - no response - URL: %s, %s"),
			*TapLogPrivacy::Url(URL), *Status);
		UE_LOG(LogTapHttp, Error, TEXT("  bWasSuccessful: %s"), bWasSuccessful ? TEXT("true") : TEXT("false"));
		UE_LOG(LogTapHttp, Error, TEXT("  Response.IsValid(): %s"), TEXT("false"));

		OnComplete.ExecuteIfBound(Result);
		return;
	}

	// Try to parse JSON response
	if (!Result.Content.IsEmpty())
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Result.Content);
		if (FJsonSerializer::Deserialize(Reader, Result.JsonObject) && Result.JsonObject.IsValid())
		{
			UE_LOG(LogTapHttp, Verbose, TEXT("Successfully parsed JSON response"));
		}
		else
		{
			UE_LOG(LogTapHttp, Warning, TEXT("Failed to parse response as JSON, treating as plain text"));
		}
	}

	// 每次响应都用 server now 校准全局时钟. 对齐 Android TapTime.calibrateServerTime:
	// Compliance / QR 都走这条路径, 这里不调 FixTime 会导致冷启动 invalid_time 死循环 —
	// ComplianceNetwork.DoFetchByTapTokenOnce 虽然在 invalid_time 时用 ServerNow 重签 MAC,
	// 但 X-Tap-Ts header 仍由 TapBasic 用本地时间填充, 重试还是 invalid_time.
	// 在这里统一 FixTime 后, 第一次响应即把时钟校准, 后续所有请求的 X-Tap-Ts 都是校准过的.
	if (Result.JsonObject.IsValid())
	{
		int64 ServerTime = 0;
		if (Result.JsonObject->TryGetNumberField(TEXT("now"), ServerTime) && ServerTime > 0)
		{
			FTapHttpTime::FixTime(ServerTime);
		}
	}

	// Check for HTTP errors
	if (!Result.IsSuccess())
	{
		Result.ErrorMessage = FString::Printf(TEXT("HTTP %d: %s"), Result.StatusCode, *Result.Content);

		// 5xx 才是真"服务端异常", 打 Warning; 4xx 经常是协议层预期响应 (例如 OAuth2 device flow
		// 的 authorization_pending / slow_down 在用户扫码确认前会一直 400, 业务侧 404 not_found
		// 表示首次用户没有记录等), 不应该刷成 warning 让人误以为出错. 业务层会根据 error 字段
		// 自己决定是否要 warn.
		if (Result.StatusCode >= 500)
		{
			UE_LOG(LogTapHttp, Warning, TEXT("HTTP Error: HTTP %d: %s"),
				Result.StatusCode, *TapLogPrivacy::Body(Result.Content));
		}
		else
		{
			UE_LOG(LogTapHttp, Log, TEXT("HTTP %d response: %s"),
				Result.StatusCode, *TapLogPrivacy::Body(Result.Content));
		}
	}

	OnComplete.ExecuteIfBound(Result);
}

TSharedRef<IHttpRequest> FTapHttpRequest::CreateRequest(
	const FString& Method,
	const FString& URL,
	float Timeout)
{
	TSharedRef<IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetVerb(Method);
	HttpRequest->SetURL(URL);
	HttpRequest->SetTimeout(Timeout);

	// Set default headers
	HttpRequest->SetHeader(TEXT("User-Agent"), TEXT("TapSDK-UE5/1.0.0"));
	HttpRequest->SetHeader(TEXT("Accept"), TEXT("application/json"));

	return HttpRequest;
}

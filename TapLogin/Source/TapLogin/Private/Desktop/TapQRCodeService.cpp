// Copyright TapTap. All Rights Reserved.

#include "TapQRCodeService.h"
#include "Internal/TapLogPrivacy.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapLoginModule.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/Http/TapHttpRequest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Containers/Ticker.h"
#include "Async/Async.h"

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapQRCodeService::FTapQRCodeService()
	: bPolling(false)
	, bPollRequestInFlight(false)
	, LastPollTime(0.0)
	, QRCodeExpireTime(0.0)
{
}

FTapQRCodeService::~FTapQRCodeService()
{
	StopPolling();
}

// =============================================================================
// Region URL (参照 Unity Region.cs)
// =============================================================================

FString FTapQRCodeService::GetWebHost() const
{
	const FTapTapSdkOptions& Config = FTapSdkConfigState::GetCurrentConfig();
	if (FTapSdkConfigState::IsRNDEnvironment())
	{
		return (Config.Region == ETapRegion::Global)
			? TEXT("https://oauth.api.xdrnd.com")
			: TEXT("https://oauth.api.xdrnd.cn");
	}
	return (Config.Region == ETapRegion::Global)
		? TEXT("https://accounts.tapapis.com")
		: TEXT("https://accounts.tapapis.cn");
}

FString FTapQRCodeService::GetDeviceInfo() const
{
	// 参照 Unity: {"device_id":"..."}
	FString DeviceId = FPlatformMisc::GetDeviceId();
	return FString::Printf(TEXT("{\"device_id\":\"%s\"}"), *DeviceId);
}

// =============================================================================
// 请求 QR 码 (参照 Unity LoginService.GetQRCodeUrl)
// =============================================================================

void FTapQRCodeService::RequestQRCode(const FString& ClientID, const FString& Scopes,
	FOnQRCodeReady OnReady)
{
	FString Endpoint = GetWebHost() + TEXT("/oauth2/v1/device/code");

	// 获取 TapLogin 版本
	FString Version = TEXT("1.0");
	// 尝试从模块获取版本
	if (FModuleManager::Get().IsModuleLoaded(TEXT("TapLogin")))
	{
		// 使用编译时定义的版本宏
#ifdef TapLogin_VERSION
		Version = TapLogin_VERSION;
#endif
	}

	FString FormBody = FString::Printf(
		TEXT("client_id=%s&response_type=device_code&scope=%s&version=%s&platform=ue&info=%s"),
		*FGenericPlatformHttp::UrlEncode(ClientID),
		*FGenericPlatformHttp::UrlEncode(Scopes),
		*FGenericPlatformHttp::UrlEncode(Version),
		*FGenericPlatformHttp::UrlEncode(GetDeviceInfo())
	);

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));

	UE_LOG(LogTemp, Warning, TEXT("[QRService] Requesting QR code... URL: %s"), *Endpoint);
	UE_LOG(LogTemp, Warning, TEXT("[QRService] Request body: %s"), *TapLogPrivacy::Body(FormBody));

	FTapHttpRequest::Request(
		TEXT("POST"),
		Endpoint,
		Headers,
		FormBody,
		FTapHttpRequestCompleteDelegate::CreateLambda([this, OnReady](const FTapHttpResponse& Result)
		{
			// HTTP 回调可能在非游戏线程, 需要切回游戏线程操作成员变量和 delegate
			int32 StatusCode = Result.StatusCode;
			FString ErrorMessage = Result.ErrorMessage;
			bool bSuccess = Result.IsSuccess();
			TSharedPtr<FJsonObject> JsonObject = Result.JsonObject;

			AsyncTask(ENamedThreads::GameThread, [this, OnReady, StatusCode, ErrorMessage, bSuccess, JsonObject]()
			{
				UE_LOG(LogTemp, Verbose, TEXT("[QRService] Response - HTTP %d"), StatusCode);

				if (bSuccess && JsonObject.IsValid())
				{
					// 服务端响应有 data 包装层: {"data":{...}, "success":true}
					const TSharedPtr<FJsonObject>* DataObj = nullptr;
					TSharedPtr<FJsonObject> Source = JsonObject;
					if (JsonObject->TryGetObjectField(TEXT("data"), DataObj) && DataObj && (*DataObj).IsValid())
					{
						Source = *DataObj;
					}

					QRCodeData.DeviceCode = Source->GetStringField(TEXT("device_code"));
					QRCodeData.QRCodeUrl = Source->GetStringField(TEXT("qrcode_url"));
					QRCodeData.ExpiresIn = Source->GetIntegerField(TEXT("expires_in"));
					QRCodeData.Interval = Source->GetIntegerField(TEXT("interval"));

					if (QRCodeData.Interval <= 0)
					{
						QRCodeData.Interval = 5;
					}

					QRCodeExpireTime = FPlatformTime::Seconds() + QRCodeData.ExpiresIn;

					UE_LOG(LogTapLogin, Log, TEXT("[QRService] QR code ready - expires: %ds, interval: %ds"),
						QRCodeData.ExpiresIn, QRCodeData.Interval);

					OnReady.ExecuteIfBound(true, QRCodeData);
				}
				else
				{
					UE_LOG(LogTapLogin, Error, TEXT("[QRService] QR code request failed: %s (HTTP %d)"),
						*ErrorMessage, StatusCode);
					OnReady.ExecuteIfBound(false, QRCodeData);
				}
			});
		}),
		30.0f
	);
}

// =============================================================================
// 轮询扫码状态 (参照 Unity QRCodeController.ListenScanQRCode)
// =============================================================================

void FTapQRCodeService::StartPolling(const FString& ClientID,
	FOnQRCodeStatusChanged OnStatusChanged,
	FOnQRCodeAuthSuccess OnAuthSuccess)
{
	StopPolling();

	PollClientID = ClientID;
	PollStatusCallback = OnStatusChanged;
	PollAuthCallback = OnAuthSuccess;
	bPolling = true;
	bPollRequestInFlight = false;
	LastPollTime = 0.0;

	// 使用 FTSTicker (不依赖 World), 存储 handle 以便 StopPolling 时显式移除
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateRaw(this, &FTapQRCodeService::PollTick));

	UE_LOG(LogTapLogin, Log, TEXT("[QRService] Started polling (interval: %ds after response)"), QRCodeData.Interval);
}

void FTapQRCodeService::StopPolling()
{
	if (bPolling)
	{
		bPolling = false;
		bPollRequestInFlight = false;
		if (TickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
			TickerHandle.Reset();
		}
		UE_LOG(LogTapLogin, Log, TEXT("[QRService] Stopped polling"));
	}
}

bool FTapQRCodeService::PollTick(float DeltaTime)
{
	if (!bPolling)
	{
		return false; // 停止 ticker
	}

	double Now = FPlatformTime::Seconds();

	// 检查过期
	if (Now >= QRCodeExpireTime)
	{
		bPolling = false;
		UE_LOG(LogTapLogin, Log, TEXT("[QRService] QR code expired"));
		PollStatusCallback.ExecuteIfBound(ETapQRCodeStatus::Expired);
		return false;
	}

	// 等上一次 response 回来后再按 interval 发下一枪；飞行中不叠请求
	if (!bPollRequestInFlight && Now - LastPollTime >= QRCodeData.Interval)
	{
		PollOnce();
	}

	return true; // 继续 ticker
}

void FTapQRCodeService::PollOnce()
{
	if (bPollRequestInFlight)
	{
		return;
	}
	bPollRequestInFlight = true;

	FString Endpoint = GetWebHost() + TEXT("/oauth2/v1/token");

	FString FormBody = FString::Printf(
		TEXT("grant_type=device_token&client_id=%s&secret_type=hmac-sha-1&code=%s&version=1.0&platform=ue&info=%s"),
		*FGenericPlatformHttp::UrlEncode(PollClientID),
		*FGenericPlatformHttp::UrlEncode(QRCodeData.DeviceCode),
		*FGenericPlatformHttp::UrlEncode(GetDeviceInfo())
	);

	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));

	FTapHttpRequest::Request(
		TEXT("POST"),
		Endpoint,
		Headers,
		FormBody,
		FTapHttpRequestCompleteDelegate::CreateLambda([WeakSelf = TWeakPtr<FTapQRCodeService>(AsShared())](const FTapHttpResponse& Result)
		{
			// HTTP 回调可能在非游戏线程, 捕获需要的数据后切回游戏线程
			int32 StatusCode = Result.StatusCode;
			FString Content = Result.Content;
			FString ErrorMessage = Result.ErrorMessage;
			TSharedPtr<FJsonObject> JsonObject = Result.JsonObject;

			AsyncTask(ENamedThreads::GameThread, [WeakSelf, StatusCode, Content, ErrorMessage, JsonObject]()
			{
				// WeakPtr 保护：若 QRService 已被销毁则安全退出，避免 use-after-free
				TSharedPtr<FTapQRCodeService> Self = WeakSelf.Pin();
				if (!Self.IsValid() || !Self->bPolling) return;

				Self->bPollRequestInFlight = false;
				Self->LastPollTime = FPlatformTime::Seconds();

				if (JsonObject.IsValid())
				{
					// 服务端响应可能有 data 包装层: {"data":{...}, "success":true}
					const TSharedPtr<FJsonObject>* DataObj = nullptr;
					TSharedPtr<FJsonObject> Source = JsonObject;
					if (JsonObject->TryGetObjectField(TEXT("data"), DataObj) && DataObj && (*DataObj).IsValid())
					{
						Source = *DataObj;
					}

					double IntervalNumber = 0;
					if (Source->TryGetNumberField(TEXT("interval"), IntervalNumber))
					{
						const int32 NewInterval = static_cast<int32>(IntervalNumber);
						if (NewInterval > 0)
						{
							Self->QRCodeData.Interval = NewInterval;
						}
					}

					// 检查是否有 error 字段 (轮询状态)
					FString Error;
					if (Source->TryGetStringField(TEXT("error"), Error))
					{
						if (Error == TEXT("authorization_pending"))
						{
							Self->PollStatusCallback.ExecuteIfBound(ETapQRCodeStatus::Pending);
						}
						else if (Error == TEXT("authorization_waiting"))
						{
							Self->PollStatusCallback.ExecuteIfBound(ETapQRCodeStatus::Scanned);
						}
						else if (Error == TEXT("access_denied"))
						{
							Self->StopPolling();
							Self->PollStatusCallback.ExecuteIfBound(ETapQRCodeStatus::Denied);
						}
						else
						{
							UE_LOG(LogTapLogin, Warning, TEXT("[QRService] Unknown poll error: %s"), *Error);
						}
					}
					else if (Source->HasField(TEXT("kid")) || Source->HasField(TEXT("access_token")))
					{
						// 成功, 解析 token
						FTapTokenData TokenData;
						TokenData.Kid = Source->GetStringField(TEXT("kid"));
						TokenData.AccessToken = Source->GetStringField(TEXT("access_token"));
						TokenData.TokenType = Source->GetStringField(TEXT("token_type"));
						TokenData.MacKey = Source->GetStringField(TEXT("mac_key"));
						TokenData.MacAlgorithm = Source->GetStringField(TEXT("mac_algorithm"));
						FString ScopeStr;
						if (Source->TryGetStringField(TEXT("scope"), ScopeStr) && !ScopeStr.IsEmpty())
						{
							ScopeStr.ParseIntoArray(TokenData.Scopes, TEXT(" "), /*CullEmpty*/ true);
						}

						UE_LOG(LogTapLogin, Log, TEXT("[QRService] Auth success - kid: %s"), TEXT("[REDACTED]"));
						Self->StopPolling();
						Self->PollStatusCallback.ExecuteIfBound(ETapQRCodeStatus::Success);
						Self->PollAuthCallback.ExecuteIfBound(TokenData);
					}
					else
					{
						UE_LOG(LogTapLogin, Warning, TEXT("[QRService] Poll response unknown format: %s"), *TapLogPrivacy::Body(Content));
					}
				}
				else
				{
					UE_LOG(LogTapLogin, Warning, TEXT("[QRService] Poll request failed: %s (HTTP %d)"), *ErrorMessage, StatusCode);
				}
			});
		}),
		15.0f
	);
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

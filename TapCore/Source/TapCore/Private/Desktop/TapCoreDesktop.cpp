// Copyright TapTap. All Rights Reserved.

#include "Platform/TapCoreDesktop.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapCore.h"
#include "Internal/TapCoreInternal.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/TapSdkChecker.h"
#include "TapMessage.h"
#include "Internal/Http/TapHttp.h"
#include "Internal/Http/TapHttpException.h"
#include "Internal/Http/TapHttpErrorConstants.h"
#include "Internal/Http/TapHttpRetryStrategy.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/App.h"
#include "Dom/JsonObject.h"
#include "TapJsonCompat.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Internal/Platform/TapAppDuration.h"
#include "Desktop/Openlog/TapOpenlogNativeAPI.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapDesktop, Log, All);

namespace
{
	FString GetClientSettingsPath(const FTapTapSdkOptions& Config)
	{
		return FPaths::Combine(
			FPaths::ProjectSavedDir(),
			FString::Printf(TEXT("TapSDKClientSettings_%s.json"), *Config.ClientID));
	}

	bool TryGetHeartbeat(const TSharedPtr<FJsonObject>& Data, bool& OutHeartbeat)
	{
		if (!Data.IsValid())
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* SwitchObj = nullptr;
		if (!Data->TryGetObjectField(TEXT("switch"), SwitchObj) || !SwitchObj || !(*SwitchObj).IsValid())
		{
			return false;
		}

		return (*SwitchObj)->TryGetBoolField(TEXT("heartbeat"), OutHeartbeat);
	}

	void ApplyHeartbeat(bool bHeartbeat)
	{
		if (bHeartbeat)
		{
			FTapAppDuration::Enable();
		}
		else
		{
			FTapAppDuration::Disable();
		}
	}
}

void FTapCoreDesktop::ApplyCachedHeartbeat(const FTapTapSdkOptions& Config)
{
	FString CachedSettings;
	const FString SettingsPath = GetClientSettingsPath(Config);
	if (!FFileHelper::LoadFileToString(CachedSettings, *SettingsPath))
	{
		return;
	}

	TSharedPtr<FJsonObject> Data;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(CachedSettings);
	if (!FJsonSerializer::Deserialize(Reader, Data))
	{
		UE_LOG(LogTapDesktop, Warning, TEXT("Failed to parse cached Gatekeeper settings: %s"), *SettingsPath);
		return;
	}

	bool bHeartbeat = false;
	if (TryGetHeartbeat(Data, bHeartbeat))
	{
		UE_LOG(LogTapDesktop, Log, TEXT("Applying cached Gatekeeper heartbeat: %s"),
			bHeartbeat ? TEXT("enabled") : TEXT("disabled"));
		ApplyHeartbeat(bHeartbeat);
	}
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

FTapCoreDesktop::FTapCoreDesktop()
	: bIsInitialized(false)
{
	UE_LOG(LogTapDesktop, Log, TEXT("TapCore Desktop implementation created"));
}

FTapCoreDesktop::~FTapCoreDesktop()
{
	// 析构时日志系统可能已被销毁（如点击窗口 X 退出），不能调用 UE_LOG
}

// =============================================================================
// ITapCorePlatform Interface Implementation
// =============================================================================

void FTapCoreDesktop::Init(const FTapTapSdkOptions& Config)
{
	if (bIsInitialized)
	{
		UE_LOG(LogTapDesktop, Warning, TEXT("Desktop platform already initialized"));
		return;
	}

	UE_LOG(LogTapDesktop, Log, TEXT("Initializing TapCore Desktop with ClientID: %s"), *Config.ClientID);

	// Store config
	CurrentConfig = Config;

	// Initialize HTTP-based communication
	InitializeHTTP(Config);

	bIsInitialized = true;
	UE_LOG(LogTapDesktop, Log, TEXT("TapCore Desktop initialized successfully"));
}

FString FTapCoreDesktop::GetPlatformVersion() const
{
	return FTapOpenlogNativeAPI::Load() ? FTapOpenlogNativeAPI::GetVersion() : FString();
}

bool FTapCoreDesktop::IsInitialized() const
{
	return bIsInitialized;
}

FString FTapCoreDesktop::GetPlatformName() const
{
	#if PLATFORM_WINDOWS
		return TEXT("Windows");
	#elif PLATFORM_MAC
		return TEXT("Mac");
	#elif PLATFORM_LINUX
		return TEXT("Linux");
	#else
		return TEXT("Desktop");
	#endif
}

void FTapCoreDesktop::UpdateLanguage(ETapLanguage Language)
{
	UE_LOG(LogTapDesktop, Log, TEXT("Updating language to: %d"), static_cast<int32>(Language));

	// Desktop platform: send language update request to server if needed
	// For now, just log the update
	UE_LOG(LogTapDesktop, Log, TEXT("Language updated successfully"));
}

// =============================================================================
// Private Methods
// =============================================================================

void FTapCoreDesktop::InitializeHTTP(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapDesktop, Log, TEXT("Initializing HTTP communication for Desktop platform"));

	// Desktop platform typically uses HTTP API instead of native SDK
	// Send initialization request to server if needed
	if (Config.bEnableLog)
	{
		UE_LOG(LogTapDesktop, Log, TEXT("Desktop mode: Using HTTP API for TapSDK operations"));
	}

	// Optional: Send init request to server
	// SendInitRequest(Config);
}

void FTapCoreDesktop::SendInitRequest(const FTapTapSdkOptions& Config)
{
	// Get base URL using the centralized method
	FString BaseUrl = FTapCoreInternal::GetBaseUrl(FTapSdkConfigState::IsRNDEnvironment(), Config.Region);
	FString ServerURL = BaseUrl + TEXT("/v1/init");

	// Create HTTP request
	TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(ServerURL);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-Client-ID"), Config.ClientID);
	Request->SetHeader(TEXT("X-Client-Token"), Config.ClientToken);

	// Build request body
	FString RequestBody = FString::Printf(TEXT(
		"{"
		"\"clientId\":\"%s\","
		"\"clientToken\":\"%s\","
		"\"region\":%d,"
		"\"gameVersion\":\"%s\","
		"\"platform\":\"Desktop\""
		"}"
	),
		*Config.ClientID,
		*Config.ClientToken,
		static_cast<int32>(Config.Region),
		*Config.GameVersion
	);

	Request->SetContentAsString(RequestBody);

	// Set response callback
	Request->OnProcessRequestComplete().BindLambda([](
		FHttpRequestPtr HttpRequest,
		FHttpResponsePtr HttpResponse,
		bool bSucceeded)
	{
		if (bSucceeded && HttpResponse.IsValid())
		{
			int32 ResponseCode = HttpResponse->GetResponseCode();
			FString ResponseBody = HttpResponse->GetContentAsString();

			if (ResponseCode == 200)
			{
				UE_LOG(LogTapDesktop, Log, TEXT("Desktop init request successful: %s"), *ResponseBody);
			}
			else
			{
				UE_LOG(LogTapDesktop, Warning, TEXT("Desktop init request returned code %d: %s"),
					ResponseCode, *ResponseBody);
			}
		}
		else
		{
			UE_LOG(LogTapDesktop, Error, TEXT("Desktop init request failed"));
		}
	});

	// Send request
	Request->ProcessRequest();
	UE_LOG(LogTapDesktop, Log, TEXT("Desktop init request sent to %s"), *ServerURL);
}

void FTapCoreDesktop::RequestGatekeeperValidation(const FTapTapSdkOptions& Config)
{
	UE_LOG(LogTapDesktop, Log, TEXT("Sending Gatekeeper validation request..."));

	// 对齐 Unity TapCoreStandalone.requestClientSetting()
	// 使用 FTapHttp 封装，自动带上 client_id query 参数、公共请求头和签名

#ifndef TapCore_VERSION
#define TapCore_VERSION TEXT("0.0.0")
#endif

	GatekeeperHttpClient = FTapHttp::NewBuilder(TEXT("TapSDKCore"), TapCore_VERSION)
		->ConnectTimeout(1.0f)
		->ReadTimeout(1.0f)
		->WriteTimeout(1.0f)
		->Build();

	// 构建请求体 - 对齐 Unity: { "platform": "pc", "bundle_id": bundleIdentifier }
	TSharedPtr<FJsonObject> RequestObj = MakeShared<FJsonObject>();
	RequestObj->SetStringField(TEXT("platform"), TEXT("pc"));

	// 从项目设置读取 bundle_id（Android PackageName 或 iOS BundleIdentifier）
	// FPlatformProcess::GetGameBundleId() 在 Mac 上会追加项目名后缀，不能直接使用
	FString BundleId;
	if (GConfig)
	{
		GConfig->GetString(TEXT("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings"), TEXT("PackageName"), BundleId, GEngineIni);
		if (BundleId.IsEmpty())
		{
			GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("BundleIdentifier"), BundleId, GEngineIni);
		}
	}
	RequestObj->SetStringField(TEXT("bundle_id"), BundleId);

	// Gatekeeper follows Android: one request, no retry, one-second timeout.
	auto RetryStrategy = FTapHttpRetryStrategy::CreateNone();
	const FString ClientSettingsPath = GetClientSettingsPath(Config);
	GatekeeperHttpClient->PostJson(
		TEXT("sdk-core/v1/gatekeeper"),
		TMap<FString, FString>(),
		RequestObj,
		false,
		RetryStrategy,
		FTapHttpJsonResultDelegate::CreateLambda([this, ClientSettingsPath](const FTapHttpJsonResult& Result)
		{
			if (Result.IsSuccess())
			{
				UE_LOG(LogTapDesktop, Log, TEXT("Gatekeeper validation successful"));

				TSharedPtr<FJsonObject> Data = Result.GetData();
				if (Data.IsValid())
				{
					FString DataStr;
					TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DataStr);
					FJsonSerializer::Serialize(Data.ToSharedRef(), Writer);
					UE_LOG(LogTapDesktop, Log, TEXT("Gatekeeper data: %s"), *DataStr);

					// Gatekeeper heartbeat controls the optional app_duration module.
					// Missing or malformed switch data is treated as disabled, matching
					// Unity's nullable switch check and avoiding accidental telemetry.
					bool bHeartbeat = false;
					if (!TryGetHeartbeat(Data, bHeartbeat))
					{
						UE_LOG(LogTapDesktop, Warning,
							TEXT("Gatekeeper response has no valid switch.heartbeat; disabling app duration"));
					}
					ApplyHeartbeat(bHeartbeat);

					if (!FFileHelper::SaveStringToFile(DataStr, *ClientSettingsPath))
					{
						UE_LOG(LogTapDesktop, Warning,
							TEXT("Failed to cache Gatekeeper settings: %s"), *ClientSettingsPath);
					}

					// Gatekeeper response structure (mirrors Unity TapGatekeeper):
					//   data.urls.{key}.browser  — URL opened in external browser
					//   data.urls.{key}.webview  — URL opened in in-app webview
					//   data.urls.{key}.uri      — deep-link URI
					// Store the .browser value keyed by the url key name.
					const TSharedPtr<FJsonObject>* UrlsObj = nullptr;
					if (Data->TryGetObjectField(TEXT("urls"), UrlsObj) && UrlsObj)
					{
						for (auto& UrlEntry : (*UrlsObj)->Values)
						{
							const FString UrlKey = TapJson::KeyString(UrlEntry.Key);
							const TSharedPtr<FJsonObject>* UrlFields = nullptr;
							if (UrlEntry.Value.IsValid() &&
								UrlEntry.Value->TryGetObject(UrlFields) && UrlFields)
							{
								FString BrowserUrl;
								if ((*UrlFields)->TryGetStringField(TEXT("browser"), BrowserUrl) &&
									!BrowserUrl.IsEmpty())
								{
									GatekeeperConfig.Add(UrlKey, BrowserUrl);
									UE_LOG(LogTapDesktop, Log,
										TEXT("[Gatekeeper] url stored: %s = %s"),
										*UrlKey, *BrowserUrl);
								}
							}
						}
					}
				}
				else
				{
					UE_LOG(LogTapDesktop, Warning,
						TEXT("Gatekeeper response has no data; disabling app duration"));
					FTapAppDuration::Disable();
				}
				FTapCoreInternal::NotifyInitSuccess(0, true);
			}
			else
			{
				TSharedPtr<FTapHttpException> Exception = Result.GetException();
				if (Exception.IsValid())
				{
					UE_LOG(LogTapDesktop, Warning, TEXT("Gatekeeper validation failed: %s"), *Exception->GetMessage());

					// 对齐 Unity: ERROR_INVALID_CLIENT 时标记 ClientInfo 不匹配，拦截后续所有 API 调用
					if (Exception->GetErrorType() == ETapHttpErrorType::ServerError)
					{
						auto ServerException = StaticCastSharedPtr<FTapHttpServerException>(Exception);
						if (ServerException.IsValid() &&
							ServerException->GetErrorData().Error == FTapHttpErrorConstants::ERROR_INVALID_CLIENT)
						{
							FTapSdkChecker::SetClientInfoMatched(false);
							UE_LOG(LogTapDesktop, Error, TEXT("Init Failed: %s"), *ServerException->GetErrorData().ErrorDescription);
							FTapMessage::ShowMessage(ServerException->GetErrorData().Msg);
							FTapCoreInternal::NotifyInitFailed(1001, ServerException->GetErrorData().Msg);
							return;
						}
					}
				}
				else
				{
					UE_LOG(LogTapDesktop, Warning, TEXT("Gatekeeper validation failed with unknown error"));
				}

				FTapCoreInternal::NotifyInitSuccess();
			}
		})
	);

	UE_LOG(LogTapDesktop, Log, TEXT("Gatekeeper request sent (single attempt, 1s timeout)"));
}

FString FTapCoreDesktop::GetGatekeeperConfigUrl(const FString& Key) const
{
	const FString* Found = GatekeeperConfig.Find(Key);
	return Found ? *Found : FString();
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

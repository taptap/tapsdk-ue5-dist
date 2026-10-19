// Copyright TapTap. All Rights Reserved.

#include "TapCore.h"
#include "TapCorePlatform.h"
#include "Async/Async.h"
#include "Init/TapInitTaskExecutor.h"
#include "Internal/Http/TapHttp.h"
#include "Internal/Http/TapHttpRequest.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Internal/TapDBInternal.h"
#include "TapTapEvent.h"
#include "Internal/TapSdkChecker.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/TapCoreInternal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Internal/Platform/TapOpenlog.h"
#include "Platform/TapCoreTracker.h"
#endif

// LogTapCore 已在 TapCoreModule.cpp 中定义

// =============================================================================
// Static Member Initialization
// =============================================================================

bool FTapTapSDK::bIsInitialized = false;
bool FTapTapSDK::bInitInProgress = false;
uint64 FTapTapSDK::InitAttemptId = 0;
int32 FTapTapSDK::InitErrorCode = 0;
FString FTapTapSDK::InitErrorMessage;
TArray<ITapInitCallback*> FTapTapSDK::InitCallbacks;
FCriticalSection FTapTapSDK::InitCallbackLock;
TUniquePtr<ITapCorePlatform> FTapTapSDK::PlatformImpl = nullptr;

// =============================================================================
// Platform Implementation Factory
// =============================================================================

void FTapTapSDK::CreatePlatformImpl()
{
	if (PlatformImpl.IsValid())
	{
		UE_LOG(LogTapCore, Warning, TEXT("Platform implementation already exists"));
		return;
	}

	// Create platform-specific implementation based on compile-time platform
	#if PLATFORM_ANDROID
		PlatformImpl = MakeUnique<FTapCoreAndroid>();
		UE_LOG(LogTapCore, Log, TEXT("Created Android platform implementation"));

	#elif PLATFORM_IOS
		PlatformImpl = MakeUnique<FTapCoreiOS>();
		UE_LOG(LogTapCore, Log, TEXT("Created iOS platform implementation"));

	#elif PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
		PlatformImpl = MakeUnique<FTapCoreDesktop>();
		UE_LOG(LogTapCore, Log, TEXT("Created Desktop platform implementation"));

	#else
		#error "Unsupported platform for TapSDK"
	#endif

	if (!PlatformImpl.IsValid())
	{
		UE_LOG(LogTapCore, Error, TEXT("Failed to create platform implementation"));
	}
}

// =============================================================================
// Public API Implementation
// =============================================================================

void FTapTapSDK::Init(const FTapTapSdkOptions& Config)
{
	InitConfigured(Config);
}

void FTapTapSDK::Init(const FTapTapSdkOptions& Config, const TArray<FTapSdkOption>& OtherOptions)
{
	FTapTapSdkOptions Configured = Config;
	FTapSdkConfigState::SetModuleOptions(Configured, OtherOptions);
	InitConfigured(Configured);
}

bool FTapTapSDK::BeginInitialization()
{
	FScopeLock Lock(&InitCallbackLock);
	if (bInitInProgress || bIsInitialized)
	{
		UE_LOG(LogTapCore, Warning, TEXT("TapSDK initialization is already in progress or completed"));
		return false;
	}
	++InitAttemptId;
	bInitInProgress = true;
	bIsInitialized = false;
	InitErrorCode = 0;
	InitErrorMessage.Empty();
	FTapSdkChecker::SetInitializationState(false, true);
	return true;
}

void FTapTapSDK::InitConfigured(const FTapTapSdkOptions& Config)
{
	const FTapTapEventOptions& EventOptions = FTapSdkConfigState::GetTapEventOptions(Config);
	const bool bIsRND = FTapSdkConfigState::IsRNDEnvironment();

	if (!BeginInitialization())
	{
		return;
	}

	UE_LOG(LogTapCore, Log, TEXT("================================================="));
	UE_LOG(LogTapCore, Log, TEXT("TapSDK Initialization Started"));
	UE_LOG(LogTapCore, Log, TEXT("Platform: %s"), TapPlatform::GetName());
	UE_LOG(LogTapCore, Log, TEXT("Version: %s"), *GetVersion());
	UE_LOG(LogTapCore, Log, TEXT("================================================="));

	// Validate configuration
	if (Config.ClientID.IsEmpty())
	{
		FTapCoreInternal::NotifyInitFailed(1000, TEXT("ClientID cannot be empty"));
		return;
	}

	if (Config.ClientToken.IsEmpty())
	{
		FTapCoreInternal::NotifyInitFailed(1000, TEXT("ClientToken cannot be empty"));
		return;
	}

	// Save configuration
	FTapSdkConfigState::SetCurrentConfig(Config);

	// Print configuration
	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("TapSDK Init Configuration:"));
	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("ClientID: %s"), *Config.ClientID);
	UE_LOG(LogTapCore, Log, TEXT("Region: %s"), Config.Region == ETapRegion::CN ? TEXT("CN") : TEXT("Global"));
	UE_LOG(LogTapCore, Log, TEXT("EnableLog: %s"), Config.bEnableLog ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTapCore, Log, TEXT("EnableTapTapEvent: %s"), EventOptions.bEnableTapTapEvent ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTapCore, Log, TEXT("GameVersion: %s"), *Config.GameVersion);
	UE_LOG(LogTapCore, Log, TEXT("IsRND: %s"), bIsRND ? TEXT("true") : TEXT("false"));
	UE_LOG(LogTapCore, Log, TEXT("ScreenOrientation: %s"), Config.ScreenOrientation == 1 ? TEXT("Landscape") : TEXT("Portrait"));

	// Get and log the base URL that will be used
	FString BaseUrl = FTapCoreInternal::GetBaseUrl(bIsRND, Config.Region);
	UE_LOG(LogTapCore, Log, TEXT("BaseURL: %s"), *BaseUrl);

	if (!EventOptions.PropertiesJson.IsEmpty())
	{
		UE_LOG(LogTapCore, Log, TEXT("EventProperties: %s"), *EventOptions.PropertiesJson);
	}
	UE_LOG(LogTapCore, Log, TEXT("========================================"));

	// Create platform implementation
	CreatePlatformImpl();

	if (!PlatformImpl.IsValid())
	{
		FTapCoreInternal::NotifyInitFailed(1003, TEXT("Failed to create platform implementation"));
		return;
	}

	// Initialize platform-specific implementation
	PlatformImpl->Init(Config);

	// Initialize TapDB and TapTapEvent
	FTapDBInternal::Init(Config);

	// Initialize OpenLog (Desktop only), independently of the TapTap Event switch.
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	FTapOpenlog::Init();
	FTapCoreDesktop::ApplyCachedHeartbeat(Config);
	FTapCoreTracker::Get().TrackInit();
#endif

	// Initialize HTTP network library
	FTapHttp::Init(Config.ClientID, Config.ClientToken, Config.Region, Config.bEnableLog, bIsRND);
	// FTapHttpRequest（旧 HTTP 系统）仅被 TapLogin OAuth 请求使用，
	// OAuth 接口不需要 X-Tap-* 公共头和 HMAC 签名（对齐 Unity TapLoginSign）。
	// FTapHttp（新 HTTP 系统）独立管理自己的签名，不受此影响。
	FTapHttpRequest::SetEnableCommonParams(false);
	FTapHttpRequest::SetModuleInfo(TEXT("TapCore"), GetVersion());
	FTapHttpRequest::SetEnableSigning(false);

	// Gatekeeper validation is handled by platform SDK (Android/iOS)
	// Only Desktop needs explicit implementation
	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("Gatekeeper validation handled by platform SDK"));

	PlatformImpl->RequestGatekeeperValidation(Config);

	UE_LOG(LogTapCore, Log, TEXT("========================================"));

	// Execute all initialization tasks
	if (!FTapInitTaskExecutor::ExecuteAll(Config))
	{
		FTapCoreInternal::NotifyInitFailed(1003, TEXT("TapSDK initialization tasks failed"));
		PlatformImpl.Reset();
		return;
	}

	UE_LOG(LogTapCore, Log, TEXT("Local initialization completed; waiting for platform final result"));
}

FString FTapTapSDK::GetVersion()
{
	return TapCore_VERSION;
}

bool FTapTapSDK::IsInitialized()
{
	return bIsInitialized && PlatformImpl.IsValid() && PlatformImpl->IsInitialized();
}

void FTapTapSDK::AddInitCallback(ITapInitCallback* Callback)
{
	if (!Callback)
	{
		return;
	}

	bool bDispatchSuccess = false;
	bool bDispatchFailure = false;
	int32 ErrorCode = 0;
	uint64 AttemptId = 0;
	FString ErrorMessage;
	{
		FScopeLock Lock(&InitCallbackLock);
		if (!InitCallbacks.Contains(Callback))
		{
			InitCallbacks.Add(Callback);
			UE_LOG(LogTapCore, Log, TEXT("AddInitCallback registered, count=%d"), InitCallbacks.Num());
		}
		AttemptId = InitAttemptId;
		if (!bInitInProgress)
		{
			bDispatchSuccess = bIsInitialized;
			bDispatchFailure = !bIsInitialized && InitErrorCode != 0;
			ErrorCode = InitErrorCode;
			ErrorMessage = InitErrorMessage;
		}
	}

	if (bDispatchSuccess || bDispatchFailure)
	{
		AsyncTask(ENamedThreads::GameThread, [Callback, bDispatchSuccess, ErrorCode, ErrorMessage, AttemptId]()
		{
			{
				FScopeLock Lock(&InitCallbackLock);
				if (AttemptId != InitAttemptId || !InitCallbacks.Contains(Callback))
				{
					return;
				}
			}
			if (bDispatchSuccess)
			{
				Callback->OnInitSuccess();
			}
			else
			{
				Callback->OnInitFail(ErrorCode, ErrorMessage);
			}
		});
	}
}

void FTapTapSDK::RemoveInitCallback(ITapInitCallback* Callback)
{
	if (!Callback)
	{
		return;
	}
	FScopeLock Lock(&InitCallbackLock);
	InitCallbacks.Remove(Callback);
	UE_LOG(LogTapCore, Log, TEXT("RemoveInitCallback completed, count=%d"), InitCallbacks.Num());
}

FString FTapCoreInternal::GetPlatformName()
{
	return TapPlatform::GetName();
}

FString FTapCoreInternal::GetNativeSDKVersion()
{
	// The initialization page needs this before Init; constructing the platform
	// implementation does not initialize or authenticate the native SDK.
	if (!FTapTapSDK::PlatformImpl.IsValid())
	{
		FTapTapSDK::CreatePlatformImpl();
	}
	return FTapTapSDK::PlatformImpl.IsValid()
		? FTapTapSDK::PlatformImpl->GetPlatformVersion()
		: FString();
}

uint64 FTapCoreInternal::GetInitAttemptId()
{
	FScopeLock Lock(&FTapTapSDK::InitCallbackLock);
	return FTapTapSDK::InitAttemptId;
}

bool FTapCoreInternal::IsInitAttemptActive(uint64 AttemptId)
{
	FScopeLock Lock(&FTapTapSDK::InitCallbackLock);
	return FTapTapSDK::bInitInProgress && AttemptId == FTapTapSDK::InitAttemptId;
}

void FTapCoreInternal::NotifyInitResult(int32 Code, const FString& Message, uint64 AttemptId)
{
	if (Code == 0)
	{
		NotifyInitSuccess(AttemptId, true);
	}
	else if (Code == 1002)
	{
		NotifyInitSuccess(AttemptId);
	}
	else
	{
		NotifyInitFailed(Code, Message, AttemptId);
	}
}

void FTapCoreInternal::NotifyInitSuccess(uint64 AttemptId, bool bClientInfoValidated)
{
	TArray<ITapInitCallback*> Snapshot;
	{
		FScopeLock Lock(&FTapTapSDK::InitCallbackLock);
		if (!FTapTapSDK::bInitInProgress || FTapTapSDK::bIsInitialized ||
			(AttemptId != 0 && AttemptId != FTapTapSDK::InitAttemptId))
		{
			return;
		}
		FTapTapSDK::bInitInProgress = false;
		FTapTapSDK::bIsInitialized = true;
		if (FTapTapSDK::PlatformImpl) FTapTapSDK::PlatformImpl->OnInitResult(true);
		if (bClientInfoValidated) FTapSdkChecker::SetClientInfoMatched(true);
		FTapSdkChecker::SetInitializationState(true, false);
		UE_LOG(LogTapCore, Log, TEXT("NotifyInitSuccess, callbackCount=%d"), FTapTapSDK::InitCallbacks.Num());
		FTapTapSDK::InitErrorCode = 0;
		FTapTapSDK::InitErrorMessage.Empty();
		Snapshot = FTapTapSDK::InitCallbacks;
		AttemptId = FTapTapSDK::InitAttemptId;
	}
	AsyncTask(ENamedThreads::GameThread, [Snapshot, AttemptId]()
	{
		for (ITapInitCallback* Callback : Snapshot)
		{
			{
				FScopeLock Lock(&FTapTapSDK::InitCallbackLock);
				if (AttemptId != FTapTapSDK::InitAttemptId ||
					!Callback || !FTapTapSDK::InitCallbacks.Contains(Callback))
				{
					continue;
				}
			}
			UE_LOG(LogTapCore, Log, TEXT("Dispatching ITapInitCallback::OnInitSuccess"));
			Callback->OnInitSuccess();
		}
	});
}

void FTapCoreInternal::NotifyInitFailed(int32 ErrorCode, const FString& ErrorMessage, uint64 AttemptId)
{
	TArray<ITapInitCallback*> Snapshot;
	{
		FScopeLock Lock(&FTapTapSDK::InitCallbackLock);
		if (!FTapTapSDK::bInitInProgress ||
			(AttemptId != 0 && AttemptId != FTapTapSDK::InitAttemptId))
		{
			return;
		}
		FTapTapSDK::bInitInProgress = false;
		FTapTapSDK::bIsInitialized = false;
		if (FTapTapSDK::PlatformImpl) FTapTapSDK::PlatformImpl->OnInitResult(false);
		FTapSdkChecker::SetInitializationState(false, false);
		UE_LOG(LogTapCore, Error, TEXT("NotifyInitFailed, code=%d, message=%s, callbackCount=%d"), ErrorCode, *ErrorMessage, FTapTapSDK::InitCallbacks.Num());
		FTapTapSDK::InitErrorCode = ErrorCode;
		FTapTapSDK::InitErrorMessage = ErrorMessage;
		Snapshot = FTapTapSDK::InitCallbacks;
		AttemptId = FTapTapSDK::InitAttemptId;
	}
	AsyncTask(ENamedThreads::GameThread, [Snapshot, ErrorCode, ErrorMessage, AttemptId]()
	{
		for (ITapInitCallback* Callback : Snapshot)
		{
			{
				FScopeLock Lock(&FTapTapSDK::InitCallbackLock);
				if (AttemptId != FTapTapSDK::InitAttemptId ||
					!Callback || !FTapTapSDK::InitCallbacks.Contains(Callback))
				{
					continue;
				}
			}
			UE_LOG(LogTapCore, Log, TEXT("Dispatching ITapInitCallback::OnInitFail, code=%d"), ErrorCode);
			Callback->OnInitFail(ErrorCode, ErrorMessage);
		}
	});
}

void FTapTapSDK::UpdateLanguage(ETapLanguage Language)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTapCore, Warning, TEXT("TapSDK is not initialized, cannot update language"));
		return;
	}

	if (!PlatformImpl.IsValid())
	{
		UE_LOG(LogTapCore, Error, TEXT("Platform implementation is not valid"));
		return;
	}

	UE_LOG(LogTapCore, Log, TEXT("Updating language to: %d"), static_cast<int32>(Language));
	PlatformImpl->UpdateLanguage(Language);
	FTapSdkConfigState::SetPreferredLanguage(Language);
}

FString FTapCoreInternal::GetBaseUrl(bool bIsRND, ETapRegion Region)
{
	// Based on Android implementation in NetExt.kt
	if (bIsRND)
	{
		// RND environment
		if (Region == ETapRegion::CN)
		{
			return TEXT("https://tapsdk.api.xdrnd.cn");
		}
		else
		{
			return TEXT("https://tapsdk.api.xdrnd.com");
		}
	}
	else
	{
		// Production environment
		if (Region == ETapRegion::CN)
		{
			return TEXT("https://tapsdk.tapapis.cn");
		}
		else
		{
			return TEXT("https://tapsdk.tapapis.com");
		}
	}
}

FString FTapCoreInternal::GetGatekeeperConfigUrl(const FString& Key)
{
	if (!FTapTapSDK::PlatformImpl.IsValid())
	{
		return FString();
	}
	return FTapTapSDK::PlatformImpl->GetGatekeeperConfigUrl(Key);
}

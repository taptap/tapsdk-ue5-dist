// Copyright TapTap. All Rights Reserved.
#include "Internal/Launcher/TapLauncherDesktopImpl.h"

#if PLATFORM_WINDOWS

#include "Internal/Launcher/TapLauncherDllWrapper.h"
#include "Launcher/TapLauncherTickPump.h"
#include "Launcher/TapLauncherErrorDialogWidget.h"
#include "Async/Async.h"
#include "HAL/PlatformMisc.h"
#include "Misc/ScopeLock.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "Launcher/TapLauncher.h"
#include "Internal/TapLauncherInternal.h"
#include "TapMessage.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapLauncher, Log, All);

namespace
{
	// Native callback: system state changed (EventId == 1)
	// userData points to a struct { int32 state }
	void OnNativeSystemState(int32 EventId, void* UserData)
	{
		if (EventId != TapLauncherEventId::SystemStateChanged || !UserData) { return; }
		const int32 State = *static_cast<int32*>(UserData);
		AsyncTask(ENamedThreads::GameThread, [State]
		{
			UTapLauncher::OnClientStateChanged.Broadcast(
				static_cast<ETapClientState>(State));
		});
	}
}

FTapLauncherDesktopImpl& FTapLauncherDesktopImpl::Get()
{
	static FTapLauncherDesktopImpl Instance;
	return Instance;
}

void FTapLauncherDesktopImpl::Reset()
{
	auto& I = Get();
	I.bInProgress  = false;
	I.bCheckDone   = false;
	I.bCheckPassed = false;
	FScopeLock Lock(&I.CacheLock);
	I.CachedOpenId.Empty();
	I.CachedResult = ETapLauncherInitResult::Unknown;
	I.CachedErrMsg.Empty();
	I.bStateListenerRegistered = false;
}

void FTapLauncherDesktopImpl::IsLaunchedFromTapTapPC(
	FOnLauncherCheckSuccess OnSuccess,
	FOnLauncherCheckError   OnError)
{
	// Return cached result if check already completed
	if (bCheckDone)
	{
		FScopeLock Lock(&CacheLock);
		if (bCheckPassed) { OnSuccess.ExecuteIfBound(CachedOpenId); }
		else              { OnError.ExecuteIfBound(CachedResult, CachedErrMsg); }
		return;
	}

	// Prevent concurrent execution
	if (bInProgress) { return; }

	// Mark the check as started before validating configuration so dependent APIs
	// can distinguish a failed check from a check that was never requested.
	bInProgress = true;

	const FTapTapSdkOptions& Cfg = FTapSdkConfigState::GetCurrentConfig();
	if (Cfg.ClientID.IsEmpty() || Cfg.ClientPublicKey.IsEmpty())
	{
		bInProgress = false;
		FireError(ETapLauncherInitResult::Unknown,
			TEXT("ClientId and ClientPublicKey are required"), OnError);
		return;
	}

	bInProgress = true;
	RunCheckAsync(Cfg.ClientID, Cfg.ClientPublicKey, OnSuccess, OnError);
}

void FTapLauncherDesktopImpl::RunCheckAsync(
	const FString& ClientId, const FString& PubKey,
	FOnLauncherCheckSuccess OnSuccess, FOnLauncherCheckError OnError)
{
	TapLauncherDllWrapper::Load();

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[this, ClientId, PubKey, OnSuccess, OnError]()
	{
		// Step 1: Check if restart via launcher is needed
		const bool bNeedRestart = TapLauncherDllWrapper::RestartAppIfNecessary(ClientId);
		UE_LOG(LogTapLauncher, Log, TEXT("RestartAppIfNecessary=%d"), bNeedRestart ? 1 : 0);

		if (bNeedRestart)
		{
			// Game must be launched via TapTap launcher; request exit
			AsyncTask(ENamedThreads::GameThread, []
			{
				FPlatformMisc::RequestExit(false);
			});
			bInProgress = false;
			FireError(ETapLauncherInitResult::Unknown,
				TEXT("Restart via TapTap launcher required"), OnError);
			return;
		}

		// Step 2: Initialize SDK with public key
		FString ErrMsg;
		const int32 RcInt = TapLauncherDllWrapper::Init(ErrMsg, PubKey);
		// Map DLL int32 result to enum. -1 (DLL "unknown") becomes ETapLauncherInitResult::Unknown (255).
		const ETapLauncherInitResult Rc = (RcInt < 0 || RcInt > 4)
			? ETapLauncherInitResult::Unknown
			: static_cast<ETapLauncherInitResult>(static_cast<uint8>(RcInt));
		UE_LOG(LogTapLauncher, Log, TEXT("TapSDK_Init rc=%d msg=%s"), RcInt, *ErrMsg);

		if (Rc != ETapLauncherInitResult::OK)
		{
			bInProgress = false;
			FireError(Rc, ErrMsg, OnError);
			return;
		}

		// Step 3: Verify ClientId matches launcher's
		FString RemoteClientId;
		if (TapLauncherDllWrapper::GetClientId(RemoteClientId)
		    && !RemoteClientId.IsEmpty()
		    && RemoteClientId != ClientId)
		{
			bInProgress = false;
			FireError(ETapLauncherInitResult::Unknown,
				FString::Printf(TEXT("ClientId mismatch: got '%s', expected '%s'"),
				                *RemoteClientId, *ClientId),
				OnError);
			return;
		}

		// Step 4: Retrieve OpenId
		FString OpenId;
		TapLauncherDllWrapper::GetOpenId(OpenId);
		UE_LOG(LogTapLauncher, Log, TEXT("OpenId=%s"), *OpenId);

		// Step 5: Success — update state on game thread
		FireSuccess(OpenId, OnSuccess);
		bInProgress = false;
	});
}

void FTapLauncherDesktopImpl::FireSuccess(
	const FString& OpenId, FOnLauncherCheckSuccess OnSuccess)
{
	{
		FScopeLock Lock(&CacheLock);
		CachedOpenId = OpenId;
		CachedResult = ETapLauncherInitResult::OK;
	}
	bCheckPassed = true;
	bCheckDone   = true;

	AsyncTask(ENamedThreads::GameThread, [this, OpenId, OnSuccess]
	{
		// Start tick pump
		FTapLauncherTickPump::Startup();

		// Register system state callback if listeners already registered
		if (!bStateListenerRegistered && UTapLauncher::OnClientStateChanged.IsBound())
		{
			TapLauncherDllWrapper::RegisterCallback(
				TapLauncherEventId::SystemStateChanged, &OnNativeSystemState);
			bStateListenerRegistered = true;
		}

		OnSuccess.ExecuteIfBound(OpenId);
		FTapLauncherInternal::OnLauncherCheckCompleted.Broadcast(OpenId);
	});
}

bool FTapLauncherDesktopImpl::GetCachedOpenId(FString& OutOpenId) const
{
	if (!bCheckPassed || !bCheckDone)
	{
		OutOpenId.Empty();
		return false;
	}

	FScopeLock Lock(&CacheLock);
	OutOpenId = CachedOpenId;
	return !OutOpenId.IsEmpty();
}

void FTapLauncherDesktopImpl::FireError(
	ETapLauncherInitResult Code, const FString& Msg, FOnLauncherCheckError OnError)
{
	{
		FScopeLock Lock(&CacheLock);
		CachedResult = Code;
		CachedErrMsg = Msg;
	}
	bCheckPassed = false;
	bCheckDone   = true;

	AsyncTask(ENamedThreads::GameThread, [Code, Msg, OnError]
	{
		// Match Unity's dedicated connection dialog, with the existing toast as
		// a safe fallback until the game registers its Blueprint subclass.
		if (!UTapLauncherErrorDialogWidget::ShowErrorDialog(Code, Msg))
		{
			FTapMessage::ShowMessage(
				UTapLauncherErrorDialogWidget::GetErrorMessage(Code).ToString());
		}
		OnError.ExecuteIfBound(Code, Msg);
	});
}

bool FTapLauncherDesktopImpl::IsPassedCheck() const
{
	return bCheckPassed;
}

bool FTapLauncherDesktopImpl::HasStartedCheck() const
{
	return bInProgress || bCheckDone;
}

void FTapLauncherDesktopImpl::Shutdown()
{
	FTapLauncherTickPump::Shutdown();
	// Unconditionally unregister before unloading the DLL to handle the async
	// window where FireSuccess's GameThread task may not have run yet but will
	// register the callback after Shutdown returns.
	TapLauncherDllWrapper::UnregisterCallback(
		TapLauncherEventId::SystemStateChanged, &OnNativeSystemState);
	bStateListenerRegistered = false;
	TapLauncherDllWrapper::Unload();
	Reset();
}

#endif // PLATFORM_WINDOWS

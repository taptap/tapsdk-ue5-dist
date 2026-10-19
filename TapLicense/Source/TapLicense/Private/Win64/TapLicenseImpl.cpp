// Copyright TapTap. All Rights Reserved.
#include "Win64/TapLicenseImpl.h"

#if PLATFORM_WINDOWS

#include "TapLicenseModule.h"
#include "TapLicense.h"
#include "Internal/TapLicenseInternal.h"
#include "Async/Async.h"
#include "Launcher/TapLauncher.h"
#include "Internal/Launcher/TapLauncherDesktopImpl.h"
#include "Internal/Launcher/TapLauncherDllWrapper.h"
#include "taptap_api.h"
#include "Win64/TapLicenseAnalytics.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapLicenseImpl, Log, All);

#ifndef TapLicense_VERSION
#define TapLicense_VERSION TEXT("0.0.0")
#endif

// ── Static member definitions ──────────────────────────────────────────────

bool                           FTapLicenseImpl::bInitialized = false;
FTapLicenseImpl::FLicenseSuccessFn FTapLicenseImpl::CbSuccess;
FTapLicenseImpl::FLicenseFailedFn  FTapLicenseImpl::CbFailed;
FTapLicenseImpl::FDLCQueryFn       FTapLicenseImpl::CbDLCQuery;
FTapLicenseImpl::FDLCOrderFn       FTapLicenseImpl::CbDLCOrder;
FTSTicker::FDelegateHandle         FTapLicenseImpl::RegistrationRetryHandle;
FTapOpenlogTracker                 FTapLicenseImpl::Tracker(TEXT("TapLicense"), TapLicense_VERSION);
FCriticalSection                   FTapLicenseImpl::PurchaseSessionLock;
TMap<FString, FString>             FTapLicenseImpl::PurchaseSessionIds;

// ── Public API ─────────────────────────────────────────────────────────────

void FTapLicenseImpl::SetCallbacks(
	FLicenseSuccessFn OnSuccess,
	FLicenseFailedFn  OnFailed,
	FDLCQueryFn       OnDLCQuery,
	FDLCOrderFn       OnDLCOrder)
{
	CbSuccess  = MoveTemp(OnSuccess);
	CbFailed   = MoveTemp(OnFailed);
	CbDLCQuery = MoveTemp(OnDLCQuery);
	CbDLCOrder = MoveTemp(OnDLCOrder);
}

bool FTapLicenseImpl::EnsureNativeCallbacksRegistered()
{
	if (bInitialized)
	{
		return true;
	}

	if (!UTapLauncher::IsPassedLauncherCheck())
	{
		return false;
	}

	if (!TapLauncherDllWrapper::IsLoaded())
	{
		UE_LOG(LogTapLicenseImpl, Verbose,
			TEXT("[TapLicense] Native callbacks pending: taptap_api.dll is not loaded yet"));
		return false;
	}

	RegisterNativeCallbacks();
	return bInitialized;
}

void FTapLicenseImpl::RegisterNativeCallbacks()
{
	if (bInitialized || !TapLauncherDllWrapper::IsLoaded())
	{
		return;
	}

	TapLauncherDllWrapper::RegisterCallback(4001, &OnNativeLicenseChanged);
	TapLauncherDllWrapper::RegisterCallback(4002, &OnNativeDLCChanged);
	bInitialized = true;
	UE_LOG(LogTapLicenseImpl, Log, TEXT("[TapLicense] Registered EventId 4001/4002 callbacks"));
}

void FTapLicenseImpl::ScheduleRegistrationRetry()
{
	if (RegistrationRetryHandle.IsValid())
	{
		return;
	}

	RegistrationRetryHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([](float) -> bool
		{
			if (FTapLicenseImpl::EnsureNativeCallbacksRegistered())
			{
				FTapLicenseImpl::RegistrationRetryHandle.Reset();
				return false;
			}
			return true;
		}),
		0.1f);
}

void FTapLicenseImpl::TrackFailure(const FString& FuncName, const FString& SessionId,
	const FString& ErrorCode, const FString& ErrorMessage,
	const TMap<FString, FString>& Props)
{
	Tracker.TrackFailure(FuncName, SessionId, ErrorCode, ErrorMessage, Props);
}

void FTapLicenseImpl::TrackPurchaseResult(const FString& SkuId, ETapDLCPurchasedCode Status)
{
	FString SessionId;
	{
		FScopeLock Lock(&PurchaseSessionLock);
		if (const FString* Found = PurchaseSessionIds.Find(SkuId))
		{
			SessionId = *Found;
			PurchaseSessionIds.Remove(SkuId);
		}
	}

	if (SessionId.IsEmpty())
	{
		return;
	}

	const TMap<FString, FString> Props = {{TEXT("sku_id"), SkuId}};
	if (Status == ETapDLCPurchasedCode::Purchased)
	{
		Tracker.TrackSuccess(TEXT("purchaseDLC"), SessionId, Props);
	}
	else
	{
		TrackFailure(TEXT("purchaseDLC"), SessionId, TEXT("-1"), TEXT("purchase dlc failed"), Props);
	}
}

bool FTapLicenseImpl::CheckLauncherState(const FString& ApiName, const FString& SessionId,
	const TMap<FString, FString>& Props, const FString& FuncName)
{
	if (FTapLauncherDesktopImpl::Get().HasStartedCheck())
	{
		return true;
	}

	const FString ErrorMessage = FString::Printf(
		TEXT("TapLicense.%s requires UTapLauncher::IsLaunchedFromTapTapPC() to be called first"),
		*ApiName);
	TrackFailure(FuncName, SessionId, TEXT("-1"), ErrorMessage, Props);
	UE_LOG(LogTapLicenseImpl, Error, TEXT("%s"), *ErrorMessage);
	FTapLicenseInternal::ReportError(ErrorMessage);
	ensureAlwaysMsgf(false, TEXT("%s"), *ErrorMessage);
	return false;
}

void FTapLicenseImpl::Initialize()
{
	Tracker.TrackInit();
	// TapLicense may initialize before the asynchronous launcher check loads the
	// DLL. Keep retrying on the game-thread ticker until the launcher is ready.
	if (!EnsureNativeCallbacksRegistered())
	{
		ScheduleRegistrationRetry();
	}
}

void FTapLicenseImpl::Shutdown()
{
	if (RegistrationRetryHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(RegistrationRetryHandle);
		RegistrationRetryHandle.Reset();
	}

	if (bInitialized)
	{
		TapLauncherDllWrapper::UnregisterCallback(4001, &OnNativeLicenseChanged);
		TapLauncherDllWrapper::UnregisterCallback(4002, &OnNativeDLCChanged);
	}
	bInitialized = false;
	CbSuccess  = nullptr;
	CbFailed   = nullptr;
	CbDLCQuery = nullptr;
	CbDLCOrder = nullptr;
	{
		FScopeLock Lock(&PurchaseSessionLock);
		PurchaseSessionIds.Empty();
	}
}

void FTapLicenseImpl::CheckLicense(bool bForce)
{
	const TapLicenseAnalytics::FOperation Operation = TapLicenseAnalytics::MakeCheckLicenseOperation(bForce);
	const FString SessionId = TapLicenseAnalytics::NewSessionId();
	Tracker.TrackStart(Operation.FuncName, SessionId, Operation.Props);

	if (!CheckLauncherState(TEXT("CheckLicense"), SessionId, Operation.Props, Operation.FuncName))
	{
		return;
	}

	if (!UTapLauncher::IsPassedLauncherCheck())
	{
		TrackFailure(Operation.FuncName, SessionId, TEXT("-1"), TEXT("LauncherCheckFailed"), Operation.Props);
		if (CbFailed) { CbFailed(); }
		return;
	}

	if (!EnsureNativeCallbacksRegistered())
	{
		TrackFailure(Operation.FuncName, SessionId, TEXT("-1"), TEXT("NativeCallbacksUnavailable"), Operation.Props);
		if (CbFailed) { CbFailed(); }
		return;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [SessionId, Operation]
	{
		const bool bOwned = TapLauncherDllWrapper::HasLicense();
		UE_LOG(LogTapLicenseImpl, Log, TEXT("[TapLicense] CheckLicense owned=%d"), bOwned);
		AsyncTask(ENamedThreads::GameThread, [bOwned, SessionId, Operation]
		{
			if (bOwned)
			{
				Tracker.TrackSuccess(Operation.FuncName, SessionId, Operation.Props);
				if (CbSuccess) { CbSuccess(); }
			}
			else
			{
				TrackFailure(Operation.FuncName, SessionId, TEXT("0"), TEXT("NOT_PURCHASED"), Operation.Props);
				if (CbFailed) { CbFailed(); }
			}
		});
	});
}

void FTapLicenseImpl::QueryDLC(const TArray<FString>& DLCList)
{
	const TapLicenseAnalytics::FOperation Operation = TapLicenseAnalytics::MakeQueryDLCOperation(DLCList);
	const FString SessionId = TapLicenseAnalytics::NewSessionId();
	Tracker.TrackStart(Operation.FuncName, SessionId, Operation.Props);

	if (!CheckLauncherState(TEXT("QueryDLC"), SessionId, Operation.Props, Operation.FuncName))
	{
		return;
	}

	if (!UTapLauncher::IsPassedLauncherCheck())
	{
		TrackFailure(Operation.FuncName, SessionId, TEXT("-1"), TEXT("LauncherCheckFailed"), Operation.Props);
		if (CbDLCQuery) { CbDLCQuery(ETapLicenseQueryCode::Error, {}); }
		return;
	}

	if (!EnsureNativeCallbacksRegistered())
	{
		TrackFailure(Operation.FuncName, SessionId, TEXT("-1"), TEXT("NativeCallbacksUnavailable"), Operation.Props);
		if (CbDLCQuery) { CbDLCQuery(ETapLicenseQueryCode::Error, {}); }
		return;
	}

	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [DLCList, SessionId, Operation]
	{
		FTapLicenseDLCResultMap Results;
		for (const FString& Sku : DLCList)
		{
			Results.Add(Sku, TapLauncherDllWrapper::IsDLCOwned(Sku));
		}
		AsyncTask(ENamedThreads::GameThread, [Results, SessionId, Operation]
		{
			Tracker.TrackSuccess(Operation.FuncName, SessionId, Operation.Props);
			if (CbDLCQuery) { CbDLCQuery(ETapLicenseQueryCode::OK, Results); }
		});
	});
}

void FTapLicenseImpl::PurchaseDLC(const FString& DLCID)
{
	const TapLicenseAnalytics::FOperation Operation = TapLicenseAnalytics::MakePurchaseDLCOperation(DLCID);
	const FString SessionId = TapLicenseAnalytics::NewSessionId();
	Tracker.TrackStart(Operation.FuncName, SessionId, Operation.Props);

	if (!CheckLauncherState(TEXT("PurchaseDLC"), SessionId, Operation.Props, Operation.FuncName))
	{
		return;
	}

	if (!UTapLauncher::IsPassedLauncherCheck())
	{
		TrackFailure(Operation.FuncName, SessionId, TEXT("-1"), TEXT("LauncherCheckFailed"), Operation.Props);
		return;
	}
	if (!EnsureNativeCallbacksRegistered())
	{
		TrackFailure(Operation.FuncName, SessionId, TEXT("-1"), TEXT("NativeCallbacksUnavailable"), Operation.Props);
		return;
	}
	if (TapLauncherDllWrapper::ShowDLCStore(DLCID))
	{
		FScopeLock Lock(&PurchaseSessionLock);
		PurchaseSessionIds.Add(DLCID, SessionId);
	}
	else
	{
		TrackFailure(Operation.FuncName, SessionId, TEXT("-1"), TEXT("ShowStoreFailed"), Operation.Props);
	}
}

// ── Native callbacks (called from TapSDK_RunCallbacks on Game Thread) ──────

void FTapLicenseImpl::OnNativeLicenseChanged(int32 /*EventId*/, void* UserData)
{
	if (!UserData) { return; }
	const auto* Payload = static_cast<const TapGamePlayableStatusChanged*>(UserData);
	const bool bOwned = (Payload->is_playable != 0);
	UE_LOG(LogTapLicenseImpl, Log, TEXT("[TapLicense] License changed owned=%d"), bOwned);
	// Already on game thread (RunCallbacks called from FTapLauncherTickPump::Tick)
	if (bOwned) { if (CbSuccess) { CbSuccess(); } }
	else        { if (CbFailed)  { CbFailed();  } }
}

void FTapLicenseImpl::OnNativeDLCChanged(int32 /*EventId*/, void* UserData)
{
	if (!UserData) { return; }
	const auto* Payload = static_cast<const TapDLCPlayableStatusChanged*>(UserData);
	const FString SkuId = UTF8_TO_TCHAR(Payload->dlc_id);
	const ETapDLCPurchasedCode Status = (Payload->is_playable != 0)
		? ETapDLCPurchasedCode::Purchased
		: ETapDLCPurchasedCode::NotPurchased;
	TrackPurchaseResult(SkuId, Status);
	UE_LOG(LogTapLicenseImpl, Log, TEXT("[TapLicense] DLC changed sku=%s owned=%d"),
		*SkuId, (int32)Status);
	// Already on game thread
	if (CbDLCOrder) { CbDLCOrder(SkuId, Status); }
}

#endif // PLATFORM_WINDOWS

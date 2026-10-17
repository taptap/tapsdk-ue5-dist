// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapLicenseTypes.h"
#include "Internal/Platform/TapOpenlogTracker.h"
#include "Containers/Ticker.h"
#include "HAL/CriticalSection.h"

#if PLATFORM_WINDOWS

/**
 * Win64 License/DLC state machine.
 * Registers native callbacks (EventId 4001/4002) via TapLauncherDllWrapper.
 * All user-facing callbacks are dispatched on the Game Thread.
 *
 * Callbacks are stored as function pointers set by Initialize() caller.
 * UTapLicense sets them to its static Multicast Delegates in Task 7.
 */
class FTapLicenseImpl
{
public:
	using FLicenseSuccessFn = TFunction<void()>;
	using FLicenseFailedFn  = TFunction<void()>;
	using FDLCQueryFn       = TFunction<void(ETapLicenseQueryCode, const FTapLicenseDLCResultMap&)>;
	using FDLCOrderFn       = TFunction<void(const FString&, ETapDLCPurchasedCode)>;

	/**
	 * Set callback functions before calling Initialize().
	 * UTapLicense calls this in TapLicenseInitTask::Execute.
	 */
	static void SetCallbacks(
		FLicenseSuccessFn OnSuccess,
		FLicenseFailedFn  OnFailed,
		FDLCQueryFn       OnDLCQuery,
		FDLCOrderFn       OnDLCOrder);

	/** Register EventId 4001/4002 native callbacks. Call from TapLicenseInitTask. */
	static void Initialize();

	/** Unregister callbacks and release. Call from FTapLicenseModule::ShutdownModule. */
	static void Shutdown();

	/** Equivalent to Unity TapTapLicense.CheckLicense(force). */
	static void CheckLicense(bool bForce);

	/** Equivalent to Unity TapTapLicense.QueryDLC(string[]). */
	static void QueryDLC(const TArray<FString>& DLCList);

	/** Equivalent to Unity TapTapLicense.PurchaseDLC(string). */
	static void PurchaseDLC(const FString& DLCID);

private:
	// Native callback functions for TapSDK_RegisterCallback
	static void OnNativeLicenseChanged(int32 EventId, void* UserData); // EventId=4001
	static void OnNativeDLCChanged    (int32 EventId, void* UserData); // EventId=4002

	/** Register native callbacks once the launcher DLL is ready. */
	static bool EnsureNativeCallbacksRegistered();
	static void RegisterNativeCallbacks();
	static void ScheduleRegistrationRetry();

	static bool CheckLauncherState(const FString& ApiName, const FString& SessionId,
		const TMap<FString, FString>& Props, const FString& FuncName);
	static void TrackFailure(const FString& FuncName, const FString& SessionId,
		const FString& ErrorCode, const FString& ErrorMessage,
		const TMap<FString, FString>& Props = {});
	static void TrackPurchaseResult(const FString& SkuId, ETapDLCPurchasedCode Status);

	static bool            bInitialized;
	static FLicenseSuccessFn CbSuccess;
	static FLicenseFailedFn  CbFailed;
	static FDLCQueryFn       CbDLCQuery;
	static FDLCOrderFn       CbDLCOrder;
	static FTSTicker::FDelegateHandle RegistrationRetryHandle;
	static FTapOpenlogTracker Tracker;
	static FCriticalSection PurchaseSessionLock;
	static TMap<FString, FString> PurchaseSessionIds;
};

#endif // PLATFORM_WINDOWS

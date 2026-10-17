// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapLicenseTypes.h"
#include "TapLicenseCallback.h"
#include "TapLicense.generated.h"

/**
 * License & DLC ownership API.
 * 平台经 ITapLicensePlatform 工厂分发：Win64 真实实现；Mac/Linux no-op stub；
 * iOS/Android 原生桥接。公开 API 跨平台一致。
 *
 * Usage:
 *   1. Register ITapLicenseCallback and/or ITapDLCCallback instances.
 *   2. Call CheckLicense() or QueryDLC() after UTapLauncher::IsLaunchedFromTapTapPC succeeds.
 *   3. Callbacks fire for both one-time query results AND real-time ownership changes.
 *
 * Equivalent to Unity TapTapLicense.
 */
UCLASS()
class TAPLICENSE_API UTapLicense : public UObject
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable,Category="TapLicense")static void RegisterLicenseCallback(TScriptInterface<ITapLicenseCallback> Callback);
	UFUNCTION(BlueprintCallable,Category="TapLicense")static void UnregisterLicenseCallback(TScriptInterface<ITapLicenseCallback> Callback);
	UFUNCTION(BlueprintCallable,Category="TapLicense")static void RegisterDLCCallback(TScriptInterface<ITapDLCCallback> Callback);
	UFUNCTION(BlueprintCallable,Category="TapLicense")static void UnregisterDLCCallback(TScriptInterface<ITapDLCCallback> Callback);

	// ── License API（对齐 Unity TapTapLicense.CheckLicense）───────────────
	/** Must be called after UTapLauncher::IsLaunchedFromTapTapPC succeeds. */
	static void CheckLicense(bool bForce = false);

	// ── DLC API（对齐 Unity TapTapLicense.QueryDLC / PurchaseDLC）─────────
	/** Results are delivered to registered ITapDLCCallback instances. */
	static void QueryDLC(const TArray<FString>& DLCList);

	/** Opens the TapTap store page for the given DLC SKU. */
	static void PurchaseDLC(const FString& DLCID);

};

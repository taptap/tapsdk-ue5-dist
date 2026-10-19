// Copyright TapTap. All Rights Reserved.
#include "TapLicense.h"
#include "Platform/ITapLicensePlatform.h"
#include "Internal/TapSdkChecker.h"
#include "TapLicenseCallbackManager.h"
#include "Internal/TapLicenseInternal.h"

namespace
{
	TSharedPtr<ITapLicensePlatform> GetPlatform()
	{
		return FTapLicensePlatformFactory::CreatePlatform();
	}
}

void FTapLicenseInternal::SetupCallbacks()
{
	if (TSharedPtr<ITapLicensePlatform> P = GetPlatform())
	{
		P->SetupCallbacks(
			[]{ FTapLicenseCallbackManager::Get().Success(); },
			[]{ FTapLicenseCallbackManager::Get().Failed(); },
			[](ETapLicenseQueryCode Code, const FTapLicenseDLCResultMap& Results)
			{
				FTapLicenseCallbackManager::Get().Query(Code, Results);
			},
			[](const FString& Sku, ETapDLCPurchasedCode Status)
			{
				FTapLicenseCallbackManager::Get().Order(Sku, Status);
			});
	}
}
void UTapLicense::RegisterLicenseCallback(TScriptInterface<ITapLicenseCallback>C){if(!FTapSdkChecker::CheckInitialize())return;FTapLicenseCallbackManager::Get().License.Register(C.GetObject(),UTapLicenseCallback::StaticClass());}
void UTapLicense::UnregisterLicenseCallback(TScriptInterface<ITapLicenseCallback>C){if(!FTapSdkChecker::CheckInitialize())return;FTapLicenseCallbackManager::Get().License.Unregister(C.GetObject());}
void UTapLicense::RegisterDLCCallback(TScriptInterface<ITapDLCCallback>C){if(!FTapSdkChecker::CheckInitialize())return;FTapLicenseCallbackManager::Get().DLC.Register(C.GetObject(),UTapDLCCallback::StaticClass());}
void UTapLicense::UnregisterDLCCallback(TScriptInterface<ITapDLCCallback>C){if(!FTapSdkChecker::CheckInitialize())return;FTapLicenseCallbackManager::Get().DLC.Unregister(C.GetObject());}

void FTapLicenseInternal::Initialize()
{
	if (TSharedPtr<ITapLicensePlatform> P = GetPlatform())
	{
		P->Initialize();
	}
}

void FTapLicenseInternal::Shutdown()
{
	// 用 GetExisting() 而非 GetPlatform()：未初始化时不应凭空构造再立刻销毁。
	if (TSharedPtr<ITapLicensePlatform> P = FTapLicensePlatformFactory::GetExisting())
	{
		P->Shutdown();
	}
	FTapLicensePlatformFactory::ResetPlatform();
}

void UTapLicense::CheckLicense(bool bForce)
{
	if (!FTapSdkChecker::CheckInitialize()) { FTapLicenseCallbackManager::Get().Failed(); return; }
	if (TSharedPtr<ITapLicensePlatform> P = GetPlatform()) { P->CheckLicense(bForce); }
	else { FTapLicenseCallbackManager::Get().Failed(); }
}

void UTapLicense::QueryDLC(const TArray<FString>& DLCList)
{
	if (!FTapSdkChecker::CheckInitialize()) { FTapLicenseCallbackManager::Get().Query(ETapLicenseQueryCode::Error, {}); return; }
	if (TSharedPtr<ITapLicensePlatform> P = GetPlatform()) { P->QueryDLC(DLCList); }
	else { FTapLicenseCallbackManager::Get().Query(ETapLicenseQueryCode::Error, {}); }
}

void UTapLicense::PurchaseDLC(const FString& DLCID)
{
	if (!FTapSdkChecker::CheckInitialize()) { FTapLicenseCallbackManager::Get().Order(DLCID, ETapDLCPurchasedCode::ReturnError); return; }
	if (TSharedPtr<ITapLicensePlatform> P = GetPlatform()) { P->PurchaseDLC(DLCID); }
	else { FTapLicenseCallbackManager::Get().Order(DLCID, ETapDLCPurchasedCode::ReturnError); }
}

void FTapLicenseInternal::ReportError(const FString& ErrorMessage)
{
	UE_LOG(LogTemp,Error,TEXT("[TapLicense] %s"),*ErrorMessage);
#if WITH_DEV_AUTOMATION_TESTS
	FTapLicenseCallbackManager::Get().TestError.Broadcast(ErrorMessage);
#endif
	FTapLicenseCallbackManager::Get().Failed();
}

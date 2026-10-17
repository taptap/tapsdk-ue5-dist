// Copyright TapTap. All Rights Reserved.

#include "TapCompliance.h"
#include "TapComplianceModule.h"
#include "Internal/TapSdkChecker.h"
#include "Platform/ITapCompliancePlatform.h"
#include "TapComplianceCallbackManager.h"


namespace
{
TSharedPtr<ITapCompliancePlatform> GetCompliancePlatform()
{
	return FTapCompliancePlatformFactory::CreatePlatform();
}
}

// =============================================================================
// Public API Implementation
// =============================================================================

void FTapCompliance::RegisterComplianceCallback(TScriptInterface<ITapComplianceCallback> Callback)
{
	if (!FTapSdkChecker::CheckInitialize()) return;

	UE_LOG(LogTapCompliance, Log, TEXT("[FTapCompliance] RegisterComplianceCallback called"));

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		return;
	}

	auto& Manager=FTapComplianceCallbackManager::Get();const bool First=Manager.Num()==0;Manager.Register(Callback.GetObject());
	if(First) Platform->RegisterComplianceCallback([](int32 Code,const FString& ExtraJson){FTapComplianceCallbackManager::Get().Dispatch(Code,ExtraJson);});
}

void FTapCompliance::UnregisterComplianceCallback(TScriptInterface<ITapComplianceCallback> Callback)
{
	if (!FTapSdkChecker::CheckInitialize()) return;
	UE_LOG(LogTapCompliance, Log, TEXT("[FTapCompliance] UnregisterComplianceCallback called"));

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		return;
	}

	auto& Manager=FTapComplianceCallbackManager::Get();Manager.Unregister(Callback.GetObject());if(Manager.Num()==0)Platform->UnregisterComplianceCallback();
}

void FTapCompliance::Startup(const FString& UserId)
{
	if (!FTapSdkChecker::CheckInitialize()) return;

	UE_LOG(LogTapCompliance, Log, TEXT("[FTapCompliance] Startup called: UserId=%s"), *UserId);

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		return;
	}

	Platform->Startup(UserId);
}

void FTapCompliance::Exit()
{
	if (!FTapSdkChecker::CheckInitialize()) return;

	UE_LOG(LogTapCompliance, Log, TEXT("[FTapCompliance] Exit called"));

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		return;
	}

	Platform->Exit();
}

void FTapCompliance::CheckPaymentLimit(
	int64 Amount,
	TFunction<void(int32 /*Status*/, const FString& /*Title*/, const FString& /*Description*/)> Callback,
	TFunction<void(const FString&)> OnError)
{
	if (!FTapSdkChecker::CheckInitialize(false))
	{
		if (OnError)
		{
			OnError(TEXT("TapSDK not initialized"));
		}
		return;
	}

	UE_LOG(LogTapCompliance, Log, TEXT("[FTapCompliance] CheckPaymentLimit called: Amount=%lld"), Amount);

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		if (OnError)
		{
			OnError(TEXT("Platform not available"));
		}
		return;
	}

	Platform->CheckPaymentLimit(Amount, MoveTemp(Callback), MoveTemp(OnError));
}

void FTapCompliance::SubmitPayment(
	int64 Amount,
	TFunction<void(bool /*bSuccess*/, const FString& /*Message*/)> Callback)
{
	if (!FTapSdkChecker::CheckInitialize(false))
	{
		if (Callback)
		{
			Callback(false, TEXT("TapSDK not initialized"));
		}
		return;
	}

	UE_LOG(LogTapCompliance, Log, TEXT("[FTapCompliance] SubmitPayment called: Amount=%lld"), Amount);

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		if (Callback)
		{
			Callback(false, TEXT("Platform not available"));
		}
		return;
	}

	Platform->SubmitPayment(Amount, Callback);
}

int32 FTapCompliance::GetAgeRange()
{
	if (!FTapSdkChecker::CheckInitialize()) return -1;

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		return -1;
	}

	return Platform->GetAgeRange();
}

int32 FTapCompliance::GetRemainingTime()
{
	if (!FTapSdkChecker::CheckInitialize()) return -1;

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		return -1;
	}

	return Platform->GetUserRemainTime();
}

FString FTapCompliance::GetCurrentAccessToken()
{
	if (!FTapSdkChecker::CheckInitialize()) return TEXT("");

	TSharedPtr<ITapCompliancePlatform> Platform = GetCompliancePlatform();
	if (!Platform.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[FTapCompliance] Platform implementation not available"));
		return TEXT("");
	}

	return Platform->GetCurrentAccessToken();
}

// Copyright TapTap. All Rights Reserved.
#include "TapLicenseModule.h"
#include "TapLicense.h"
#include "Internal/TapLicenseInternal.h"
#include "TapLicenseCallbackManager.h"

DEFINE_LOG_CATEGORY(LogTapLicense);

#define LOCTEXT_NAMESPACE "FTapLicenseModule"

void FTapLicenseModule::StartupModule()
{
	UE_LOG(LogTapLicense, Log, TEXT("TapLicense module started"));
}

void FTapLicenseModule::ShutdownModule()
{
	UE_LOG(LogTapLicense, Log, TEXT("TapLicense module shutdown"));
	FTapLicenseCallbackManager::Get().Reset();FTapLicenseInternal::Shutdown();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTapLicenseModule, TapLicense)

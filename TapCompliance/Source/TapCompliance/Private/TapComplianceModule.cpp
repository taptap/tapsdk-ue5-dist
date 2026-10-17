// Copyright TapTap. All Rights Reserved.

#include "TapComplianceModule.h"
#include "TapComplianceCallbackManager.h"
#include "TapCompliance.h"
#include "TapComplianceOptions.h"
#include "Internal/TapSdkOptionSerializerRegistry.h"
#include "Dom/JsonObject.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "Desktop/ComplianceNetwork.h"
#include "Desktop/DesktopComplianceManager.h"
#include "Internal/TapComplianceScopeProvider.h"
#endif

DEFINE_LOG_CATEGORY(LogTapCompliance);

#define LOCTEXT_NAMESPACE "FTapComplianceModule"

void FTapComplianceModule::StartupModule()
{
	FTapSdkOptionSerializerRegistry::Get().RegisterSerializer(
		FTapComplianceOptions::StaticStruct(),
		TEXT("TapTapComplianceOptions"),
		[](const FTapSdkOption& Option, const TSharedRef<FJsonObject>& Json)
		{
			if (const FTapComplianceOptions* ComplianceOptions = Option.GetPtr<FTapComplianceOptions>())
			{
				Json->SetBoolField(TEXT("showSwitchAccount"), ComplianceOptions->bShowSwitchAccount);
				Json->SetBoolField(TEXT("useAgeRange"), ComplianceOptions->bUseAgeRange);
			}
		});
	UE_LOG(LogTapCompliance, Log, TEXT("TapCompliance module started"));
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	FTapComplianceScopeProvider::Register([]()
	{
		return TOptional<ETapLoginScope>(FDesktopComplianceManager::Get().UsesAgeRange()
			? ETapLoginScope::Compliance
			: ETapLoginScope::ComplianceBasic);
	});
	FComplianceNetwork::Initialize();
#endif
}

void FTapComplianceModule::ShutdownModule()
{
	FTapSdkOptionSerializerRegistry::Get().UnregisterSerializer(FTapComplianceOptions::StaticStruct());
	FTapComplianceCallbackManager::Get().Reset();
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	FTapComplianceScopeProvider::Unregister();
	FComplianceNetwork::Shutdown();
#endif
	UE_LOG(LogTapCompliance, Log, TEXT("TapCompliance module shutdown"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FTapComplianceModule, TapCompliance)

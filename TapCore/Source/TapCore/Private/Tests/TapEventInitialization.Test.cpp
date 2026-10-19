// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "Internal/TapDBInternal.h"
#include "Internal/TapSdkConfigState.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Platform/TapEventDesktop.h"
#include "Internal/Platform/TapOpenlog.h"
#include "Internal/Storage/TapDeviceId.h"
#include "Internal/TapDBEventTracker.h"
#include "TapTapEvent.h"

namespace
{
	// Count initialization without starting the real sender or creating events.
	class FCountingTapEventDesktop : public FTapEventDesktop
	{
	public:
		int32 InitCalls = 0;
		bool bReceivedEnableLog = false;

		virtual void Init(bool bInEnableLog) override
		{
			++InitCalls;
			bReceivedEnableLog = bInEnableLog;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapEventInitializationTest,
	"TapSDK.Core.Event.InitializationHonorsEnableSwitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapEventInitializationTest::RunTest(const FString&)
{
	TUniquePtr<ITapEventPlatform> OriginalPlatform = MoveTemp(FTapTapEvent::PlatformImpl);
	const bool bOriginalInitialized = FTapTapEvent::bIsInitialized;
	const bool bOriginalEnableLog = FTapTapEvent::bEnableLog;
	const bool bOriginalEnabled = FTapDBInternal::bIsEnabled;
	FString OriginalLocale;
	{
		FScopeLock Lock(&FTapOpenlog::DynamicVarsMutex);
		OriginalLocale = FTapOpenlog::CurrentSdkLocale;
	}
	ON_SCOPE_EXIT
	{
		FTapTapEvent::PlatformImpl = MoveTemp(OriginalPlatform);
		FTapTapEvent::bIsInitialized = bOriginalInitialized;
		FTapTapEvent::bEnableLog = bOriginalEnableLog;
		FTapDBInternal::bIsEnabled = bOriginalEnabled;
		FScopeLock Lock(&FTapOpenlog::DynamicVarsMutex);
		FTapOpenlog::CurrentSdkLocale = OriginalLocale;
	};

	auto Platform = MakeUnique<FCountingTapEventDesktop>();
	FCountingTapEventDesktop* CountingPlatform = Platform.Get();
	FTapTapEvent::PlatformImpl = MoveTemp(Platform);
	FTapTapEvent::bIsInitialized = false;
	// Ensure Init replaces a previous enabled flag before taking the disabled path.
	FTapDBInternal::bIsEnabled = true;

	FTapTapSdkOptions Config;
	Config.bEnableLog = true;
	FTapTapEventOptions EventOptions;
	EventOptions.bEnableTapTapEvent = false;
	TArray<FTapSdkOption> Options;
	Options.Add(FTapSdkOption::Make<FTapTapEventOptions>(EventOptions));
	FTapSdkConfigState::SetModuleOptions(Config, Options);
	FTapDBInternal::Init(Config);

	TestFalse(TEXT("Disabled options should update the event API gate"), FTapDBInternal::IsEnabled());
	TestFalse(TEXT("Disabled events should leave the facade uninitialized"), FTapTapEvent::bIsInitialized);
	TestEqual(TEXT("Disabled events must not initialize the platform/Tracker"), CountingPlatform->InitCalls, 0);

	const bool bTrackerWasInitialized = FTapDBEventTracker::Get().IsInitialized();
	TSharedRef<FJsonObject> Common = MakeShared<FJsonObject>();
	FTapOpenlog::CollectGeneralParameters(Common);
	const FString SharedDeviceId = FTapDeviceId::Get();
	TestFalse(TEXT("Shared device identity should remain available"), SharedDeviceId.IsEmpty());
	TestEqual(TEXT("OpenLog device_id must not depend on event initialization"),
		Common->GetStringField(TEXT("device_id")), SharedDeviceId);
	TestEqual(TEXT("OpenLog install_uuid should retain the shared device identity"),
		Common->GetStringField(TEXT("install_uuid")), SharedDeviceId);
	TestEqual(TEXT("Collecting OpenLog parameters must not start the event Tracker"),
		FTapDBEventTracker::Get().IsInitialized(), bTrackerWasInitialized);

	// With no event option supplied, the default enabled behavior is unchanged.
	FTapSdkConfigState::SetModuleOptions(Config, TArray<FTapSdkOption>());
	FTapDBInternal::Init(Config);
	TestTrue(TEXT("Events should remain enabled by default"), FTapDBInternal::IsEnabled());
	TestTrue(TEXT("Enabled events should initialize the facade"), FTapTapEvent::bIsInitialized);
	TestEqual(TEXT("Enabled events should initialize the platform once"), CountingPlatform->InitCalls, 1);
	TestTrue(TEXT("Platform initialization should preserve the log option"), CountingPlatform->bReceivedEnableLog);
	return true;
}

#endif

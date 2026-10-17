// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "TapCore.h"
#include "Internal/TapCoreInternal.h"
#include "Internal/Http/TapHttp.h"
#include "Internal/TapSdkChecker.h"
#include "Internal/Platform/ITapCorePlatform.h"
#include "Async/TaskGraphInterfaces.h"
#include "Misc/ScopeExit.h"

namespace
{
	class FRetryPlatform : public ITapCorePlatform
	{
	public:
		bool bInitialized = false;
		int32 Requests = 0;
		virtual void Init(const FTapTapSdkOptions&) override { if (!bInitialized) ++Requests; }
		virtual bool IsInitialized() const override { return bInitialized; }
		virtual void OnInitResult(bool bSuccess) override { bInitialized = bSuccess; }
		virtual FString GetPlatformVersion() const override { return TEXT("test"); }
		virtual FString GetPlatformName() const override { return TEXT("test"); }
		virtual void UpdateLanguage(ETapLanguage) override {}
		virtual void RequestGatekeeperValidation(const FTapTapSdkOptions&) override {}
	};
	class FInitResults : public ITapInitCallback
	{
	public:
		int32 Successes = 0, Failures = 0;
		virtual void OnInitSuccess() override { ++Successes; }
		virtual void OnInitFail(int32, const FString&) override { ++Failures; }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapCoreInitRetryTest,
	"TapSDK.Core.Initialization.RetryAndStaleResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapCoreInitRetryTest::RunTest(const FString&)
{
	if (FTapTapSDK::bInitInProgress)
	{
		AddWarning(TEXT("Cannot isolate initialization state while a real init is running"));
		return true;
	}
	auto OriginalPlatform = MoveTemp(FTapTapSDK::PlatformImpl);
	auto OriginalCallbacks = MoveTemp(FTapTapSDK::InitCallbacks);
	const bool bOriginalInitialized = FTapTapSDK::bIsInitialized;
	const int32 OriginalError = FTapTapSDK::InitErrorCode;
	const FString OriginalMessage = FTapTapSDK::InitErrorMessage;
	ON_SCOPE_EXIT
	{
		FTapTapSDK::InitCallbacks.Empty();
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		FTapTapSDK::PlatformImpl = MoveTemp(OriginalPlatform);
		FTapTapSDK::InitCallbacks = MoveTemp(OriginalCallbacks);
		FTapTapSDK::bIsInitialized = bOriginalInitialized;
		FTapTapSDK::bInitInProgress = false;
		FTapTapSDK::InitErrorCode = OriginalError;
		FTapTapSDK::InitErrorMessage = OriginalMessage;
		FTapSdkChecker::SetClientInfoMatched(true);
		FTapSdkChecker::SetInitializationState(bOriginalInitialized, false);
	};
	if (!FTapHttp::IsInitialized())
	{
		FTapHttp::Init(TEXT("test_client"), TEXT("test_token"), ETapRegion::CN, false);
	}
	FTapSdkChecker::SetClientInfoMatched(true);
	auto Platform = MakeUnique<FRetryPlatform>();
	auto* Probe = Platform.Get();
	FTapTapSDK::PlatformImpl = MoveTemp(Platform);
	FTapTapSDK::bIsInitialized = false;
	FTapTapSDK::InitErrorCode = 0;
	FInitResults Results;
	FTapTapSDK::AddInitCallback(&Results);
	const FTapTapSdkOptions Config;

	TestTrue(TEXT("First init starts"), FTapTapSDK::BeginInitialization());
	const uint64 First = FTapCoreInternal::GetInitAttemptId();
	Probe->Init(Config);
	TestFalse(TEXT("Accepting a request is not final success"), FTapTapSDK::IsInitialized());
	TestFalse(TEXT("Overlapping Init is rejected"), FTapTapSDK::BeginInitialization());
	AddExpectedError(TEXT("NotifyInitFailed, code=1001"), EAutomationExpectedErrorFlags::Contains, 1);
	FTapSdkChecker::SetClientInfoMatched(false);
	FTapCoreInternal::NotifyInitResult(1001, TEXT("invalid_client"), First);
	TestFalse(TEXT("Failure clears platform success"), Probe->bInitialized);
	TestFalse(TEXT("Failure ends the in-progress state"), FTapTapSDK::bInitInProgress);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("First failure is delivered"), Results.Failures, 1);

	// A late subscriber queues a replay of the old failure, then a new init begins.
	FInitResults LateSubscriber;
	FTapTapSDK::AddInitCallback(&LateSubscriber);
	TestTrue(TEXT("Failed init can retry"), FTapTapSDK::BeginInitialization());
	const uint64 Second = FTapCoreInternal::GetInitAttemptId();
	Probe->Init(Config);
	TestEqual(TEXT("Retry sends another native request"), Probe->Requests, 2);
	FTapCoreInternal::NotifyInitResult(0, TEXT("stale success"), First);
	FTapCoreInternal::NotifyInitResult(1001, TEXT("stale failure"), First);
	TestTrue(TEXT("Old results cannot finish the retry"), FTapCoreInternal::IsInitAttemptActive(Second));
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestEqual(TEXT("Old queued failure replay is suppressed"), LateSubscriber.Failures, 0);
	FTapCoreInternal::NotifyInitResult(0, FString(), Second);
	FTapCoreInternal::NotifyInitResult(1001, TEXT("duplicate failure"), Second);
	FTapCoreInternal::NotifyInitResult(0, FString(), Second);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestTrue(TEXT("Retry succeeds in both public and platform state"), FTapTapSDK::IsInitialized());
	TestTrue(TEXT("Successful retry restores business API checks"), FTapSdkChecker::CheckInitialize(false));
	TestEqual(TEXT("Final success is delivered exactly once"), Results.Successes, 1);
	TestEqual(TEXT("Late subscriber receives the new success"), LateSubscriber.Successes, 1);
	TestEqual(TEXT("Late failures do not overwrite success"), Results.Failures, 1);
	FTapTapSDK::RemoveInitCallback(&LateSubscriber);
	FTapTapSDK::RemoveInitCallback(&Results);

	// Immediate bridge rejection must also leave initialization retryable.
	FTapTapSDK::bIsInitialized = false;
	Probe->bInitialized = false;
	TestTrue(TEXT("Bridge rejection attempt starts"), FTapTapSDK::BeginInitialization());
	AddExpectedError(TEXT("NotifyInitFailed, code=1003"), EAutomationExpectedErrorFlags::Contains, 1);
	FTapCoreInternal::NotifyInitFailed(1003, TEXT("JNI unavailable"), FTapCoreInternal::GetInitAttemptId());
	TestTrue(TEXT("Immediate rejection can retry"), FTapTapSDK::BeginInitialization());
	FTapSdkChecker::SetClientInfoMatched(false);
	FTapCoreInternal::NotifyInitResult(1002, TEXT("cached fallback"), FTapCoreInternal::GetInitAttemptId());
	TestTrue(TEXT("Existing 1002 fallback remains successful"), FTapTapSDK::IsInitialized());
	AddExpectedError(TEXT("clientId"), EAutomationExpectedErrorFlags::Contains, 1);
	TestFalse(TEXT("Cached fallback does not clear an invalid-client result"), FTapSdkChecker::CheckInitialize(false));
	return true;
}
#endif

// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)
#include "CompliancePoll.h"
#include "ComplianceState.h"
#include "ComplianceConst.h"
#include "Engine/Engine.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePollDeadlineTest,
	"TapCompliance.Poll.Pure.RemainingTimeDeadline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePollDeadlineTest::RunTest(const FString&)
{
	if (FCompliancePoll::Get().IsRunning() || (GEngine && GEngine->GameViewport))
	{
		AddWarning(TEXT("Run without an active game viewport/compliance session to isolate restriction UI"));
		return true;
	}
	FTSTicker Ticker;
	FCompliancePoll Poll(Ticker);
	FComplianceState& State = FComplianceState::Get();
	const FString OldUser = State.CurrentUserId;
	const FString OldSession = State.CurrentSessionId;
	const auto OldVerification = State.Verification;
	const auto OldConfig = State.UserAntiConfig;
	const auto OldPlayable = State.CurrentPlayableResult;
	const int32 OldRemain = State.RemainSeconds.GetValue();
	const int64 OldOfflineTime = State.GetOfflineLocalUnixTimeForTest();
	const double OldMonotonicTime = State.GetOfflineBaseMonotonicSecondsForTest();
	auto OldCallbacks = State.SwapCallbacksForTest({});
	ON_SCOPE_EXIT
	{
		Poll.Logout();
		State.CurrentUserId = OldUser;
		State.CurrentSessionId = OldSession;
		State.Verification = OldVerification;
		State.UserAntiConfig = OldConfig;
		State.CurrentPlayableResult = OldPlayable;
		State.RemainSeconds.Set(OldRemain);
		State.SetOfflineLocalTimeForTest(OldOfflineTime, OldMonotonicTime);
		State.SwapCallbacksForTest(MoveTemp(OldCallbacks));
	};

	int32 Requests = 0;
	int32 Restrictions = 0;
	TArray<FComplianceNetwork::FOnPlayable> Pending;
	Poll.SendHeartbeatForTest = [&](FComplianceNetwork::FOnPlayable Callback)
	{
		++Requests;
		Pending.Add(MoveTemp(Callback));
	};
	State.RegisterCallback([&](int32 Code, const FString&)
	{
		TestEqual(TEXT("Only PERIOD_RESTRICT should be dispatched"), Code, TapComplianceConst::PERIOD_RESTRICT);
		++Restrictions;
	});
	auto Start = [&](int32 Remain, int32 Interval = 120)
	{
		Poll.Logout();
		Ticker.Reset();
		Requests = Restrictions = 0;
		Pending.Reset();
		State.CurrentUserId = TEXT("poll-deadline-test");
		State.CurrentSessionId = TEXT("poll-deadline-session");
		State.Verification = FComplianceVerification();
		State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_TEEN;
		State.UserAntiConfig = FUserAntiConfig();
		State.UserAntiConfig.Policy.Model = TapComplianceConst::POLICY_MODEL_SERVER;
		State.UserAntiConfig.Policy.Active = TEXT("duration");
		State.RemainSeconds.Set(Remain);
		Poll.StartUp(Interval);
	};
	auto Complete = [&](int32 Remain)
	{
		if (!TestEqual(TEXT("Exactly one heartbeat is pending"), Pending.Num(), 1)) return;
		auto Callback = MoveTemp(Pending[0]);
		Pending.Reset();
		FPlayableResult Result;
		Result.RemainTime = Remain;
		Callback(true, Result, FComplianceError());
	};
	auto TickSeconds = [&](int32 Seconds)
	{
		for (int32 Index = 0; Index < Seconds; ++Index) Ticker.Tick(1.0f);
	};

	// Even if countdown execution drifts, the first heartbeat must honor initial remaining time.
	Start(5);
	Poll.StopCountdownTicker();
	Ticker.Tick(5.0f);
	TestEqual(TEXT("No premature first heartbeat"), Requests, 0);
	Ticker.Tick(1.0f);
	TestEqual(TEXT("First heartbeat runs within remaining time + 1, not 120 seconds"), Requests, 1);
	Complete(600);
	Ticker.Tick(119.0f);
	TestEqual(TEXT("An extended allowance restores the normal heartbeat interval"), Requests, 1);
	Ticker.Tick(1.0f);
	TestEqual(TEXT("Normal interval continues polling"), Requests, 2);

	// Countdown expiry requests a review; it does not itself decide that play is forbidden.
	Start(5);
	TickSeconds(4);
	TestEqual(TEXT("Countdown has one second left"), State.RemainSeconds.GetValue(), 1);
	TestEqual(TEXT("No request before the boundary"), Requests, 0);
	TickSeconds(1);
	TestEqual(TEXT("Countdown zero immediately requests a heartbeat"), Requests, 1);
	TestEqual(TEXT("Await the server before restricting"), Restrictions, 0);
	TickSeconds(125);
	TestEqual(TEXT("A slow response must not produce overlapping heartbeats"), Requests, 1);
	TestEqual(TEXT("Countdown remains clamped to zero"), State.RemainSeconds.GetValue(), 0);

	// Headless runs still execute the production panel call. Count that call via its diagnostic.
	AddExpectedErrorPlain(TEXT("[PanelManager] No GameViewport available (HealthMinorTimeLimit)"),
		EAutomationExpectedErrorFlags::Contains, 3);
	if (Pending.Num() == 1)
	{
		auto Callback = Pending[0];
		FPlayableResult Denied;
		Denied.RemainTime = 0;
		Callback(true, Denied, FComplianceError());
		Callback(true, Denied, FComplianceError());
	}
	TestEqual(TEXT("Restriction response is dispatched once"), Restrictions, 1);
	TestFalse(TEXT("Restriction stops polling"), Poll.IsRunning());
	TestFalse(TEXT("Restriction removes countdown"), Poll.CountdownHandle.IsValid());
	TestFalse(TEXT("Restriction removes heartbeat"), Poll.HeartbeatHandle.IsValid());

	// Local time_range policy must re-evaluate at zero without issuing any HTTP request.
	Start(5);
	State.UserAntiConfig.Policy.Model = TapComplianceConst::POLICY_MODEL_LOCAL;
	State.UserAntiConfig.Policy.Active = TapComplianceConst::POLICY_ACTIVE_TIME_RANGE;
	// Empty holiday configuration deterministically denies play at the next local review.
	TickSeconds(5);
	TestEqual(TEXT("Local policy restricts at the countdown boundary"), Restrictions, 1);
	TestEqual(TEXT("Local policy does not send HTTP"), Requests, 0);
	TickSeconds(120);
	TestEqual(TEXT("Local restriction is not repeated"), Restrictions, 1);

	Start(5);
	State.UserAntiConfig.Policy.Active = TapComplianceConst::POLICY_ACTIVE_TIME_RANGE;
	TickSeconds(5);
	if (TestEqual(TEXT("Server time_range policy rechecks at zero"), Pending.Num(), 1))
	{
		FComplianceError Error;
		Error.TransportErrorType = ETapHttpErrorType::NetworkError;
		Pending[0](false, FPlayableResult(), Error);
	}
	TestEqual(TEXT("Failed heartbeat still applies local time_range fallback"), Restrictions, 1);

	// Completion during Pause must not restart timers; Resume must retain short deadlines.
	Start(1);
	TickSeconds(2);
	Poll.Pause();
	Complete(5);
	TestFalse(TEXT("Paused completion cannot restart heartbeat"), Poll.HeartbeatHandle.IsValid());
	Poll.Resume();
	TestEqual(TEXT("Resume performs an immediate review"), Requests, 2);
	Complete(5);
	TickSeconds(5);
	TestEqual(TEXT("Resumed allowance still rechecks at zero"), Requests, 3);
	TestEqual(TEXT("Playable responses must not restrict"), Restrictions, 0);

	Start(0);
	TickSeconds(1);
	TestEqual(TEXT("An initially exhausted allowance is reviewed promptly"), Requests, 1);
	Start(1, 1);
	TickSeconds(1);
	TestEqual(TEXT("Heartbeat and countdown due together send only one request"), Requests, 1);
	TickSeconds(3);
	TestEqual(TEXT("Concurrent deadline triggers wait for the in-flight response"), Requests, 1);
	Start(TapComplianceConst::AGE_LIMIT_UNKNOWN, 0);
	Poll.StopCountdownTicker();
	Ticker.Tick(119.0f);
	TestEqual(TEXT("Unknown allowance uses the default interval"), Requests, 0);
	Ticker.Tick(1.0f);
	TestEqual(TEXT("Invalid interval defaults to 120 seconds"), Requests, 1);
	Start(TapComplianceConst::ADULT_INFINITE_REMAIN_TIME);
	State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_ADULT;
	State.Verification.bIsAdult = true;
	TickSeconds(120);
	TestEqual(TEXT("Adult allowance keeps the policy heartbeat interval"), Requests, 1);
	TestEqual(TEXT("Adult is not restricted"), Restrictions, 0);
	return true;
}
#endif

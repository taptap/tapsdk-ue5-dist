// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)
#include "CompliancePoll.h"
#include "ComplianceState.h"
#include "ComplianceConst.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePollSessionIsolationTest,
	"TapCompliance.Poll.Pure.StaleHeartbeatIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePollSessionIsolationTest::RunTest(const FString&)
{
	if (FCompliancePoll::Get().IsRunning())
	{
		AddWarning(TEXT("Cannot isolate heartbeat state while a real compliance session is running"));
		return true;
	}
	FCompliancePoll Poll;
	FComplianceState& State = FComplianceState::Get();
	const FString OldUser = State.CurrentUserId;
	const FString OldSession = State.CurrentSessionId;
	const FString OldTrackingSession = State.TrackingSessionId;
	const auto OldVerification = State.Verification;
	const auto OldConfig = State.UserAntiConfig;
	const auto OldPlayable = State.CurrentPlayableResult;
	const int32 OldRemain = State.RemainSeconds.GetValue();
	const bool bOldManual = State.bUsedManualVerify;
	const int64 OldOfflineTime = State.GetOfflineLocalUnixTimeForTest();
	const double OldMonotonicTime = State.GetOfflineBaseMonotonicSecondsForTest();
	auto OldCallbacks = State.SwapCallbacksForTest({});
	ON_SCOPE_EXIT
	{
		Poll.Logout();
		State.CurrentUserId = OldUser;
		State.CurrentSessionId = OldSession;
		State.TrackingSessionId = OldTrackingSession;
		State.Verification = OldVerification;
		State.UserAntiConfig = OldConfig;
		State.CurrentPlayableResult = OldPlayable;
		State.RemainSeconds.Set(OldRemain);
		State.bUsedManualVerify = bOldManual;
		State.SetOfflineLocalTimeForTest(OldOfflineTime, OldMonotonicTime);
		State.SwapCallbacksForTest(MoveTemp(OldCallbacks));
	};
	int32 Events = 0;
	State.RegisterCallback([&](int32, const FString&) { ++Events; });
	State.CurrentUserId = TEXT("heartbeat-test-A-") + FComplianceState::GenerateSession32();
	State.CurrentSessionId = FComplianceState::GenerateSession32();
	Poll.StartUp(120);

	// Capture the same production completion functions passed to the HTTP layer,
	// then deliver them after the old session has ended. No network or ticker waits.
	auto OldSuccess = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	auto OldRestriction = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	auto OldExpired = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	auto OldNetworkError = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	Poll.Logout();
	const FString UserB = TEXT("heartbeat-test-B-") + FComplianceState::GenerateSession32();
	const FString SessionB = FComplianceState::GenerateSession32();
	State.CurrentUserId = UserB;
	State.CurrentSessionId = SessionB;
	State.Verification.ComplianceToken = TEXT("current-session-token");
	State.CurrentPlayableResult.RemainTime = 900;
	State.RemainSeconds.Set(900);
	Poll.StartUp(120);

	FPlayableResult StaleResult;
	StaleResult.RemainTime = 300;
	OldSuccess(true, StaleResult, FComplianceError());
	TestEqual(TEXT("Old success must not change B's remaining time"), State.RemainSeconds.GetValue(), 900);
	TestEqual(TEXT("Old success must not replace B's playable state"), State.CurrentPlayableResult.RemainTime, 900);
	StaleResult.RemainTime = 0;
	OldRestriction(true, StaleResult, FComplianceError());
	FComplianceError Expired;
	Expired.Error = TEXT("business_code_error");
	Expired.ErrorCode = 200000;
	TestTrue(TEXT("Fixture exercises the token-expired branch"), Expired.IsTokenExpired());
	OldExpired(false, FPlayableResult(), Expired);
	FComplianceError NetworkError;
	NetworkError.TransportErrorType = ETapHttpErrorType::NetworkError;
	OldNetworkError(false, FPlayableResult(), NetworkError);
	TestTrue(TEXT("Old failure/restriction must not stop B's polling"), Poll.IsRunning());
	TestEqual(TEXT("Old token expiry must not log out B"), State.CurrentUserId, UserB);
	TestEqual(TEXT("Old token expiry must not clear B's token"), State.Verification.ComplianceToken, FString(TEXT("current-session-token")));
	TestEqual(TEXT("Old failures must not mutate remaining time"), State.RemainSeconds.GetValue(), 900);
	TestEqual(TEXT("Old responses must not dispatch exit/restriction/error events"), Events, 0);

	FPlayableResult CurrentResult;
	CurrentResult.RemainTime = 600;
	auto Current = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	Current(true, CurrentResult, FComplianceError());
	TestEqual(TEXT("Current heartbeat still updates remaining time"), State.RemainSeconds.GetValue(), 600);
	TestEqual(TEXT("Current heartbeat still updates playable state"), State.CurrentPlayableResult.RemainTime, 600);

	// A new polling lifetime must invalidate requests even if both identity strings match.
	auto BeforeRestart = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	Poll.StartUp(120);
	BeforeRestart(false, FPlayableResult(), Expired);
	TestTrue(TEXT("Repeated StartUp invalidates the previous lifetime"), Poll.IsRunning());
	TestEqual(TEXT("Same-identity restart preserves the current user"), State.CurrentUserId, UserB);
	auto BeforeLogout = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	Poll.Logout();
	BeforeLogout(true, CurrentResult, FComplianceError());
	TestEqual(TEXT("Response after Logout cannot restore state"), State.RemainSeconds.GetValue(), TapComplianceConst::AGE_LIMIT_UNKNOWN);
	Poll.StartUp(120);
	State.RemainSeconds.Set(700);
	BeforeLogout(true, CurrentResult, FComplianceError());
	TestEqual(TEXT("Logout and re-entry with the same identity rejects old success"), State.RemainSeconds.GetValue(), 700);

	// Identity may change before polling is restarted; both fields must still match.
	auto BeforeSessionChange = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	State.CurrentSessionId = FComplianceState::GenerateSession32();
	BeforeSessionChange(false, FPlayableResult(), Expired);
	auto BeforeUserChange = Poll.CreateHeartbeatCallback(State.CurrentUserId, State.CurrentSessionId);
	State.CurrentUserId = TEXT("heartbeat-test-C-") + FComplianceState::GenerateSession32();
	BeforeUserChange(false, FPlayableResult(), NetworkError);
	TestTrue(TEXT("Identity mismatch cannot stop the active poll"), Poll.IsRunning());
	TestEqual(TEXT("Identity mismatch cannot change remaining time"), State.RemainSeconds.GetValue(), 700);
	TestEqual(TEXT("Only stale errors were delivered, so no events should fire"), Events, 0);
	return true;
}
#endif

// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "ComplianceConst.h"
#include "ComplianceState.h"
#include "DesktopComplianceManager.h"
#include "Platform/TapCompliancePlatformDesktop.h"

namespace
{
	bool IsLowerHex32(const FString& Value)
	{
		if (Value.Len() != 32)
		{
			return false;
		}
		for (const TCHAR Ch : Value)
		{
			const bool bDigit = Ch >= TEXT('0') && Ch <= TEXT('9');
			const bool bLowerHex = Ch >= TEXT('a') && Ch <= TEXT('f');
			if (!bDigit && !bLowerHex)
			{
				return false;
			}
		}
		return true;
	}

	class FScopedComplianceUserState
	{
	public:
		explicit FScopedComplianceUserState(FComplianceState& InState)
			: State(InState)
			, OldCurrentUserId(InState.CurrentUserId)
			, OldCurrentSessionId(InState.CurrentSessionId)
			, OldTrackingSessionId(InState.TrackingSessionId)
			, OldClientId(InState.ClientId)
			, OldVerification(InState.Verification)
			, OldUserAntiConfig(InState.UserAntiConfig)
			, OldRealNameConfig(InState.RealNameConfig)
			, OldCurrentPlayableResult(InState.CurrentPlayableResult)
			, OldRemainSeconds(InState.RemainSeconds.GetValue())
			, OldOfflineLocalUnixTime(InState.GetOfflineLocalUnixTimeForTest())
			, OldOfflineBaseMonotonicSeconds(InState.GetOfflineBaseMonotonicSecondsForTest())
			, bOldShowSwitchAccount(InState.bShowSwitchAccount)
			, bOldUseAgeRange(InState.bUseAgeRange)
			, bOldUsedManualVerify(InState.bUsedManualVerify)
		{
		}

		~FScopedComplianceUserState()
		{
			State.CurrentUserId = OldCurrentUserId;
			State.CurrentSessionId = OldCurrentSessionId;
			State.TrackingSessionId = OldTrackingSessionId;
			State.ClientId = OldClientId;
			State.Verification = OldVerification;
			State.UserAntiConfig = OldUserAntiConfig;
			State.RealNameConfig = OldRealNameConfig;
			State.CurrentPlayableResult = OldCurrentPlayableResult;
			State.RemainSeconds.Set(OldRemainSeconds);
			State.SetOfflineLocalTimeForTest(OldOfflineLocalUnixTime, OldOfflineBaseMonotonicSeconds);
			State.bShowSwitchAccount = bOldShowSwitchAccount;
			State.bUseAgeRange = bOldUseAgeRange;
			State.bUsedManualVerify = bOldUsedManualVerify;
		}

	private:
		FComplianceState& State;
		FString OldCurrentUserId;
		FString OldCurrentSessionId;
		FString OldTrackingSessionId;
		FString OldClientId;
		FComplianceVerification OldVerification;
		FUserAntiConfig OldUserAntiConfig;
		FRealNameConfigResult OldRealNameConfig;
		FPlayableResult OldCurrentPlayableResult;
		int32 OldRemainSeconds;
		int64 OldOfflineLocalUnixTime;
		double OldOfflineBaseMonotonicSeconds;
		bool bOldShowSwitchAccount;
		bool bOldUseAgeRange;
		bool bOldUsedManualVerify;
	};

	class FScopedComplianceCallbacks
	{
	public:
		explicit FScopedComplianceCallbacks(FComplianceState& InState)
			: State(InState)
			, OldCallbacks(State.SwapCallbacksForTest(TArray<FComplianceState::FComplianceEventCallback>()))
		{
		}

		~FScopedComplianceCallbacks()
		{
			State.SwapCallbacksForTest(MoveTemp(OldCallbacks));
		}

	private:
		FComplianceState& State;
		TArray<FComplianceState::FComplianceEventCallback> OldCallbacks;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceStateGenerateSession32Test,
	"TapCompliance.State.Pure.GenerateSession32",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceStateGenerateSession32Test::RunTest(const FString& /*Parameters*/)
{
	TSet<FString> Seen;
	for (int32 Index = 0; Index < 128; ++Index)
	{
		const FString Session = FComplianceState::GenerateSession32();
		TestTrue(TEXT("session should be 32 lowercase hex chars"), IsLowerHex32(Session));
		TestFalse(TEXT("session should be unique in a tight loop"), Seen.Contains(Session));
		Seen.Add(Session);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceStateAgeRangeExposureTest,
	"TapCompliance.State.Pure.AgeRangeExposure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceStateAgeRangeExposureTest::RunTest(const FString& /*Parameters*/)
{
	FComplianceState& State = FComplianceState::Get();
	const bool OldUseAgeRange = State.bUseAgeRange;
	const FComplianceVerification OldVerification = State.Verification;

	State.bUseAgeRange = false;
	State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_ADULT;
	TestEqual(TEXT("useAgeRange=false hides age"), State.GetExposedAgeRange(), TapComplianceConst::AGE_LIMIT_UNKNOWN);

	State.bUseAgeRange = true;
	State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_SENTINEL_MINOR;
	TestEqual(TEXT("minor sentinel is hidden"), State.GetExposedAgeRange(), TapComplianceConst::AGE_LIMIT_UNKNOWN);

	State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_SENTINEL_ADULT;
	TestEqual(TEXT("adult sentinel is hidden"), State.GetExposedAgeRange(), TapComplianceConst::AGE_LIMIT_UNKNOWN);

	State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_ADULT;
	TestEqual(TEXT("real age range is exposed"), State.GetExposedAgeRange(), TapComplianceConst::AGE_LIMIT_ADULT);

	State.bUseAgeRange = OldUseAgeRange;
	State.Verification = OldVerification;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceStateRemainTimeContractTest,
	"TapCompliance.State.Pure.RemainTimeContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceStateRemainTimeContractTest::RunTest(const FString& /*Parameters*/)
{
	FComplianceState& State = FComplianceState::Get();
	FScopedComplianceUserState UserStateGuard(State);

	State.RemainSeconds.Set(TapComplianceConst::AGE_LIMIT_UNKNOWN);
	TestEqual(TEXT("negative remain time is exposed as zero before a result"), FDesktopComplianceManager::Get().GetUserRemainTime(), 0);

	State.RemainSeconds.Set(-5);
	TestEqual(TEXT("any negative remain time is normalized to zero"), FDesktopComplianceManager::Get().GetUserRemainTime(), 0);

	State.RemainSeconds.Set(30);
	TestEqual(TEXT("positive remain time is exposed unchanged"), FDesktopComplianceManager::Get().GetUserRemainTime(), 30);

	State.ResetUserState();
	TestEqual(TEXT("reset user state returns remain time to zero"), FDesktopComplianceManager::Get().GetUserRemainTime(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceStateOfflineLocalTimeBaselineTest,
	"TapCompliance.State.Pure.OfflineLocalTimeBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceStateOfflineLocalTimeBaselineTest::RunTest(const FString& /*Parameters*/)
{
	FComplianceState& State = FComplianceState::Get();
	FScopedComplianceUserState UserStateGuard(State);

	State.SetOfflineLocalTimeForTest(1000, FPlatformTime::Seconds() - 5.0);
	const int64 First = State.GetOfflineLocalUnixTime();
	TestTrue(TEXT("offline local time advances from monotonic baseline"), First >= 1004 && First <= 1006);

	State.ResetOfflineLocalTimeIfNeeded();
	const int64 Second = State.GetOfflineLocalUnixTime();
	TestTrue(TEXT("reset is idempotent once baseline exists"), Second >= 1004 && Second <= 1007);

	State.ResetUserState();
	TestEqual(TEXT("reset user state clears offline baseline"), State.GetOfflineLocalUnixTimeForTest(), static_cast<int64>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceStateCallbackRegistryTest,
	"TapCompliance.State.Pure.CallbackRegistry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceStateCallbackRegistryTest::RunTest(const FString& /*Parameters*/)
{
	FComplianceState& State = FComplianceState::Get();
	FScopedComplianceCallbacks CallbackGuard(State);

	int32 CallCount = 0;
	int32 LastCode = 0;
	FString LastExtra;
	State.RegisterCallback([&CallCount, &LastCode, &LastExtra](int32 Code, const FString& ExtraJson)
	{
		++CallCount;
		LastCode = Code;
		LastExtra = ExtraJson;
	});

	State.DispatchCallback(TapComplianceConst::LOGIN_SUCCESS, TEXT("{\"ok\":true}"));
	TestEqual(TEXT("registered callback is invoked once"), CallCount, 1);
	TestEqual(TEXT("callback receives code"), LastCode, TapComplianceConst::LOGIN_SUCCESS);
	TestEqual(TEXT("callback receives extra json"), LastExtra, FString(TEXT("{\"ok\":true}")));

	State.SwapCallbacksForTest(TArray<FComplianceState::FComplianceEventCallback>());
	AddExpectedErrorPlain(TEXT("[State] DispatchCallback Code=1000 but no listener registered"),
		EAutomationExpectedErrorFlags::Contains, 1);
	State.DispatchCallback(TapComplianceConst::EXITED, FString());
	TestEqual(TEXT("unregistered callback is not invoked again"), CallCount, 1);
	return true;
}

// =============================================================================
// Bug fix coverage:
//   FTapCompliancePlatformDesktop::Exit() 必须派发 EXITED 回调
//   (对齐 Unity ComplianceNewJob.Exit() 行为).
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceManagerExitDispatchesEXITEDTest,
	"TapCompliance.Manager.Pure.ExitDispatchesEXITED",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceManagerExitDispatchesEXITEDTest::RunTest(const FString& /*Parameters*/)
{
	// Bug 1 修复回归: Exit() 必须派发 EXITED, 否则业务方 UI 卡在"已通过"状态.
	// 直接复用 FTapCompliancePlatformDesktop 入口, 验证回调链路完整.
	FComplianceState& State = FComplianceState::Get();
	FScopedComplianceCallbacks CallbackGuard(State);

	int32 ReceivedCode = 0;
	int32 CallCount    = 0;
	State.RegisterCallback([&ReceivedCode, &CallCount](int32 Code, const FString& /*Extra*/)
	{
		++CallCount;
		ReceivedCode = Code;
	});

	// 走平台入口, 真实触发 Logout(true) + DispatchCallback(EXITED).
	// (Logout 内部对 CurrentUserId 为空的情况无副作用, 满足 pure 测试约束.)
	FTapCompliancePlatformDesktop Platform;
	Platform.Exit();

	TestEqual(TEXT("Exit() should invoke callback exactly once"), CallCount,    1);
	TestEqual(TEXT("Exit() should dispatch EXITED(1000)"),        ReceivedCode, TapComplianceConst::EXITED);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

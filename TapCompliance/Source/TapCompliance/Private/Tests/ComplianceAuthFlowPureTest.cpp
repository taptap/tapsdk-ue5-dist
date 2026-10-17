// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "ComplianceAuthFlow.h"
#include "ComplianceConst.h"
#include "ComplianceState.h"

namespace
{
	class FScopedAuthFlowState
	{
	public:
		FScopedAuthFlowState()
			: State(FComplianceState::Get())
			, OldVerification(State.Verification)
			, OldUserAntiConfig(State.UserAntiConfig)
		{
		}

		~FScopedAuthFlowState()
		{
			State.Verification = OldVerification;
			State.UserAntiConfig = OldUserAntiConfig;
		}

	private:
		FComplianceState& State;
		FComplianceVerification OldVerification;
		FUserAntiConfig OldUserAntiConfig;
	};

	FUserAntiConfig MakeReminderConfig()
	{
		FUserAntiConfig Config;
		FTimeRangeConfig& TimeRange = Config.LocalConfig.TimeRangeConfig;
		TimeRange.UITipText.Allow.TipTitle = TEXT("允许游玩");
		TimeRange.UITipText.Allow.TipDescription = TEXT("今日总时长 60 分钟，还可玩 # ${remaining} # 分钟");
		TimeRange.UITipText.Reject.TipTitle = TEXT("暂不可玩");
		TimeRange.UITipText.Reject.TipDescription = TEXT("当前不可进入游戏");
		return Config;
	}

	void SetMinorTimeRange(FComplianceState& State, const FDateTime& Now, const FString& Start, const FString& End)
	{
		State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_TEEN;
		State.Verification.bIsAdult = false;

		FTimeRangeConfig& TimeRange = State.UserAntiConfig.LocalConfig.TimeRangeConfig;
		TimeRange.Holidays.Reset();
		TimeRange.Holidays.Add(Now.ToString(TEXT("%Y-%m-%d")));
		TimeRange.TimeStart = Start;
		TimeRange.TimeEnd = End;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceAuthFlowShouldUseLocalOnlyTest,
	"TapCompliance.AuthFlow.Pure.ShouldUseLocalOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceAuthFlowShouldUseLocalOnlyTest::RunTest(const FString& /*Parameters*/)
{
	FScopedAuthFlowState StateGuard;
	FComplianceState& State = FComplianceState::Get();

	State.UserAntiConfig.Policy.Model = TapComplianceConst::POLICY_MODEL_LOCAL;
	State.UserAntiConfig.Policy.Active = TapComplianceConst::POLICY_ACTIVE_TIME_RANGE;
	TestTrue(TEXT("local + time_range uses local-only policy"), FComplianceAuthFlow::ShouldUseLocalOnly());

	State.UserAntiConfig.Policy.Model = TapComplianceConst::POLICY_MODEL_SERVER;
	TestFalse(TEXT("server model does not use local-only policy"), FComplianceAuthFlow::ShouldUseLocalOnly());

	State.UserAntiConfig.Policy.Model = TapComplianceConst::POLICY_MODEL_LOCAL;
	State.UserAntiConfig.Policy.Active = TEXT("duration");
	TestFalse(TEXT("non time_range active policy does not use local-only policy"), FComplianceAuthFlow::ShouldUseLocalOnly());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceAuthFlowResolveServerTextWinsTest,
	"TapCompliance.AuthFlow.Pure.ResolveHealthReminderText.ServerTextWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceAuthFlowResolveServerTextWinsTest::RunTest(const FString& /*Parameters*/)
{
	FPlayableResult Playable;
	Playable.RemainTime = 30;
	Playable.Title = TEXT("服务端标题");
	Playable.Content = TEXT("服务端内容");

	FText Title;
	FText Content;
	FComplianceAuthFlow::ResolveHealthReminderText(Playable, MakeReminderConfig(), Title, Content);

	TestEqual(TEXT("server title wins"), Title.ToString(), FString(TEXT("服务端标题")));
	TestEqual(TEXT("server content wins"), Content.ToString(), FString(TEXT("服务端内容")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceAuthFlowResolveLocalAllowMinutesTest,
	"TapCompliance.AuthFlow.Pure.ResolveHealthReminderText.LocalAllowMinutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceAuthFlowResolveLocalAllowMinutesTest::RunTest(const FString& /*Parameters*/)
{
	FPlayableResult Playable;
	Playable.RemainTime = 61;

	FText Title;
	FText Content;
	FComplianceAuthFlow::ResolveHealthReminderText(Playable, MakeReminderConfig(), Title, Content);

	TestEqual(TEXT("allow title fallback"), Title.ToString(), FString(TEXT("允许游玩")));
	TestEqual(TEXT("remain seconds rounds up to minutes"), Content.ToString(),
		FString(TEXT("今日总时长 60 分钟，还可玩 2 分钟")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceAuthFlowResolveLocalAllowSecondsTest,
	"TapCompliance.AuthFlow.Pure.ResolveHealthReminderText.LocalAllowSeconds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceAuthFlowResolveLocalAllowSecondsTest::RunTest(const FString& /*Parameters*/)
{
	FPlayableResult Playable;
	Playable.RemainTime = 30;

	FText Title;
	FText Content;
	FComplianceAuthFlow::ResolveHealthReminderText(Playable, MakeReminderConfig(), Title, Content);

	TestEqual(TEXT("allow title fallback"), Title.ToString(), FString(TEXT("允许游玩")));
	TestEqual(TEXT("only text after placeholder switches minutes to seconds"), Content.ToString(),
		FString(TEXT("今日总时长 60 分钟，还可玩 30 秒")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceAuthFlowComputeOfflineAdultInfiniteTest,
	"TapCompliance.AuthFlow.Pure.ComputeOfflinePlayable.AdultInfinite",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceAuthFlowComputeOfflineAdultInfiniteTest::RunTest(const FString& /*Parameters*/)
{
	FScopedAuthFlowState StateGuard;
	FComplianceState& State = FComplianceState::Get();
	State.Verification.AgeLimit = TapComplianceConst::AGE_LIMIT_ADULT;
	State.Verification.bIsAdult = true;

	const FPlayableResult Result = FComplianceAuthFlow::ComputeOfflinePlayableAt(FDateTime(2026, 6, 2, 20, 30, 0));
	TestEqual(TEXT("adult has infinite remain time"), Result.RemainTime,
		TapComplianceConst::ADULT_INFINITE_REMAIN_TIME);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceAuthFlowComputeOfflineMinorOutsideWindowTest,
	"TapCompliance.AuthFlow.Pure.ComputeOfflinePlayable.MinorOutsideWindowDenied",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceAuthFlowComputeOfflineMinorOutsideWindowTest::RunTest(const FString& /*Parameters*/)
{
	FScopedAuthFlowState StateGuard;
	FComplianceState& State = FComplianceState::Get();
	const FDateTime Now(2026, 6, 2, 19, 59, 0);
	SetMinorTimeRange(State, Now, TEXT("20:00"), TEXT("21:00"));

	const FPlayableResult Result = FComplianceAuthFlow::ComputeOfflinePlayableAt(Now);
	TestEqual(TEXT("minor outside allowed window is denied"), Result.RemainTime, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceAuthFlowComputeOfflineMinorInsideWindowTest,
	"TapCompliance.AuthFlow.Pure.ComputeOfflinePlayable.MinorInsideWindowAllowed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceAuthFlowComputeOfflineMinorInsideWindowTest::RunTest(const FString& /*Parameters*/)
{
	FScopedAuthFlowState StateGuard;
	FComplianceState& State = FComplianceState::Get();
	const FDateTime Now(2026, 6, 2, 20, 30, 0);
	SetMinorTimeRange(State, Now, TEXT("20:00"), TEXT("21:00"));

	const FPlayableResult Result = FComplianceAuthFlow::ComputeOfflinePlayableAt(Now);
	TestEqual(TEXT("minor inside allowed window gets remaining seconds"), Result.RemainTime, 1800);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

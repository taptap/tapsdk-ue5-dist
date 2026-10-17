// Copyright TapTap. All Rights Reserved.

#include "CompliancePoll.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceAuthFlow.h"
#include "ComplianceConst.h"
#include "ComplianceModels.h"
#include "ComplianceNetwork.h"
#include "ComplianceState.h"
#include "DesktopComplianceManager.h"
#include "TapComplianceModule.h"
#include "TapCompliancePanelManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CoreDelegates.h"

FCompliancePoll& FCompliancePoll::Get()
{
	static FCompliancePoll Instance;
	return Instance;
}

FCompliancePoll::FCompliancePoll()
{
	BindAppLifecycle();
}

FCompliancePoll::~FCompliancePoll()
{
	UnbindAppLifecycle();
	StopHeartbeatTicker();
	StopCountdownTicker();
}

// =============================================================================
// 生命周期
// =============================================================================

void FCompliancePoll::StartUp(int32 InIntervalSec)
{
	++SessionGeneration;
	IntervalSec = InIntervalSec > 0
		? InIntervalSec
		: TapComplianceConst::DEFAULT_HEARTBEAT_INTERVAL_SEC;

	bRunning = true;
	bPaused  = false;

	UE_LOG(LogTapCompliance, Log, TEXT("[Poll] Start, interval=%d sec"), IntervalSec);

	EnsureCountdownTicker();
	ScheduleHeartbeat(IntervalSec);
}

void FCompliancePoll::Logout()
{
	++SessionGeneration;
	UE_LOG(LogTapCompliance, Log, TEXT("[Poll] Logout"));
	bRunning = false;
	bPaused  = false;
	StopHeartbeatTicker();
	StopCountdownTicker();
	FComplianceState::Get().RemainSeconds.Set(TapComplianceConst::AGE_LIMIT_UNKNOWN);
}

void FCompliancePoll::Pause()
{
	if (!bRunning || bPaused) return;
	UE_LOG(LogTapCompliance, Log, TEXT("[Poll] Pause"));
	bPaused = true;
	StopHeartbeatTicker();
	StopCountdownTicker();
}

void FCompliancePoll::Resume()
{
	if (!bRunning || !bPaused) return;
	UE_LOG(LogTapCompliance, Log, TEXT("[Poll] Resume + immediate heartbeat"));
	bPaused = false;
	EnsureCountdownTicker();
	// 立即触发一次心跳同步，再按周期排
	TickHeartbeatOnce();
	ScheduleHeartbeat(IntervalSec);
}

// =============================================================================
// 心跳 ticker
// =============================================================================

void FCompliancePoll::ScheduleHeartbeat(int32 InIntervalSec)
{
	StopHeartbeatTicker();
	const float Period = static_cast<float>(InIntervalSec > 0 ? InIntervalSec : 1);
	HeartbeatHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[this](float /*DeltaTime*/) -> bool
			{
				if (!bRunning || bPaused)
				{
					return false; // 取消 ticker
				}
				TickHeartbeatOnce();
				return true; // 继续
			}),
		Period);
}

void FCompliancePoll::TickHeartbeatOnce()
{
	const FString UserId    = FComplianceState::Get().CurrentUserId;
	const FString SessionId = FComplianceState::Get().CurrentSessionId;
	if (UserId.IsEmpty())
	{
		UE_LOG(LogTapCompliance, Warning, TEXT("[Poll] Heartbeat skipped: no userId"));
		return;
	}

	// 对齐 Android PlayLogModel.checkUserState (PlayLogModel.kt:24): policy_model=local +
	// active=time_range 时本地直接判, 不发心跳. Step3 入口同样判一次, 此处保证轮询不绕过.
	if (FComplianceAuthFlow::ShouldUseLocalOnly())
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Poll] policy_model=local + time_range → use local calc instead of heartbeat"));
		HandlePlayableUpdate(FComplianceAuthFlow::ComputeOfflinePlayable());
		return;
	}

	FComplianceNetwork::Heartbeat(UserId, SessionId, CreateHeartbeatCallback(UserId, SessionId));
}

FComplianceNetwork::FOnPlayable FCompliancePoll::CreateHeartbeatCallback(const FString& UserId, const FString& SessionId)
{
	const uint64 RequestGeneration = SessionGeneration;
	return [this, RequestGeneration, UserId, SessionId](bool bSuccess, const FPlayableResult& Result, const FComplianceError& Err)
		{
			const FComplianceState& State = FComplianceState::Get();
			if (!bRunning || RequestGeneration != SessionGeneration ||
				UserId != State.CurrentUserId || SessionId != State.CurrentSessionId)
			{
				return;
			}

			if (bSuccess)
			{
				HandlePlayableUpdate(Result);
			}
			else
			{
				if (Err.IsTokenExpired())
				{
					UE_LOG(LogTapCompliance, Warning, TEXT("[Poll] Heartbeat token expired → EXITED"));
					Logout();
					FDesktopComplianceManager::Get().Logout(/*bClearCache=*/true);
					FDesktopComplianceManager::Get().DispatchCallback(
						TapComplianceConst::EXITED, FString());
					return;
				}

				UE_LOG(LogTapCompliance, Warning,
					TEXT("[Poll] Heartbeat failed: code=%d error=%s desc=%s"),
					Err.ErrorCode, *Err.Error, *Err.ErrorDescription);

				// 对齐 Unity CheckPlayableAsyncWithFallback (BaseComplianceWorker.cs:106) 和
				// Android PlayLogModel.uploadPlayLogSync onFail (PlayLogModel.kt:100):
				// policy_active=time_range 时心跳失败不应中止合规会话, 用本地计算继续运行
				// (周期内还会再次心跳尝试恢复).
				const FAntiPolicy& Policy = FComplianceState::Get().UserAntiConfig.Policy;
				if (Policy.Active.Equals(TapComplianceConst::POLICY_ACTIVE_TIME_RANGE))
				{
					UE_LOG(LogTapCompliance, Log,
						TEXT("[Poll] Heartbeat failed but policy_active=time_range → use offline calc, keep ticker"));
					HandlePlayableUpdate(FComplianceAuthFlow::ComputeOfflinePlayable());
					return;
				}

				// 非 time_range 策略: 没有本地兜底, 停 ticker 发 INVALID
				StopHeartbeatTicker();
				StopCountdownTicker();
				bRunning = false;
				FDesktopComplianceManager::Get().DispatchCallback(
					TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR, FString());
			}
		};
}

void FCompliancePoll::HandlePlayableUpdate(const FPlayableResult& Result)
{
	FComplianceState::Get().CurrentPlayableResult = Result;
	const int32 Remain = FMath::Max(0, Result.RemainTime);
	FComplianceState::Get().RemainSeconds.Set(Remain);

	UE_LOG(LogTapCompliance, Log, TEXT("[Poll] PlayableUpdate, RemainTime=%d"), Remain);

	if (Remain <= 0)
	{
		// 时段限制 / 时长耗尽
		StopHeartbeatTicker();
		StopCountdownTicker();
		bRunning = false;

		FDesktopComplianceManager::Get().DispatchCallback(
			TapComplianceConst::PERIOD_RESTRICT, FString());

		// "切换账号" 按钮: 仅在 bShowSwitchAccount=true 时绑定 (禁玩态)
		FOnHealthMinorTimeLimitSwitchAccount OnSwitchAccountCb;
		if (FComplianceState::Get().bShowSwitchAccount)
		{
			OnSwitchAccountCb = FOnHealthMinorTimeLimitSwitchAccount::CreateLambda([]()
			{
				FDesktopComplianceManager::Get().Logout(/*bClearCache=*/false);
				FDesktopComplianceManager::Get().DispatchCallback(
					TapComplianceConst::SWITCH_ACCOUNT, FString());
			});
		}

		// 动态文案: 心跳轮询到禁玩态时, 服务端通常会下发新的 title/description_plain;
		// 空时回退本地 UITipText.Reject (RemainTime<=0).
		FText ReminderTitle, ReminderContent;
		FComplianceAuthFlow::ResolveHealthReminderText(
			FComplianceState::Get().CurrentPlayableResult,
			FComplianceState::Get().UserAntiConfig,
			ReminderTitle,
			ReminderContent);

		FTapCompliancePanelManager::Get().ShowHealthMinorTimeLimitPanel(
			/*bAllowEnterGame=*/false,
			ReminderTitle,
			ReminderContent,
			// 与 Unity BaseComplianceWorker.cs:302 OnUnplayablePostPoll 对齐:
			// 心跳触发禁玩, "退出游戏" 按钮由 SDK 主动调 QuitGame, 不依赖游戏侧自行监听 PERIOD_RESTRICT(1030).
			FOnHealthMinorTimeLimitExitGame::CreateLambda([]()
			{
				UWorld* World = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr;
				UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
			}),
			OnSwitchAccountCb,
			FOnHealthMinorTimeLimitClosed  ::CreateLambda([](){ /* up to game */ })
		);
		return;
	}

	// 剩余时间不足一个完整心跳周期 → 缩短下一轮
	if (Remain <= IntervalSec)
	{
		ScheduleHeartbeat(Remain + 1);
	}
}

void FCompliancePoll::StopHeartbeatTicker()
{
	if (HeartbeatHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatHandle);
		HeartbeatHandle.Reset();
	}
}

// =============================================================================
// 1s 倒计时 ticker
// =============================================================================

void FCompliancePoll::EnsureCountdownTicker()
{
	StopCountdownTicker();
	CountdownHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda(
			[this](float /*DeltaTime*/) -> bool
			{
				if (!bRunning || bPaused)
				{
					return false;
				}
				FComplianceState& S = FComplianceState::Get();
				const int32 Cur = S.RemainSeconds.GetValue();
				if (Cur > 0)
				{
					S.RemainSeconds.Decrement();
				}
				return true;
			}),
		1.0f);
}

void FCompliancePoll::StopCountdownTicker()
{
	if (CountdownHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(CountdownHandle);
		CountdownHandle.Reset();
	}
}

// =============================================================================
// 前后台 (近似)
// =============================================================================

void FCompliancePoll::BindAppLifecycle()
{
	DeactivateHandle = FCoreDelegates::ApplicationWillDeactivateDelegate.AddLambda(
		[this]() { Pause(); });
	ReactivateHandle = FCoreDelegates::ApplicationHasReactivatedDelegate.AddLambda(
		[this]() { Resume(); });
}

void FCompliancePoll::UnbindAppLifecycle()
{
	if (DeactivateHandle.IsValid())
	{
		FCoreDelegates::ApplicationWillDeactivateDelegate.Remove(DeactivateHandle);
		DeactivateHandle.Reset();
	}
	if (ReactivateHandle.IsValid())
	{
		FCoreDelegates::ApplicationHasReactivatedDelegate.Remove(ReactivateHandle);
		ReactivateHandle.Reset();
	}
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

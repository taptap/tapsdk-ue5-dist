// Copyright TapTap. All Rights Reserved.

#include "DesktopComplianceManager.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceAuthFlow.h"
#include "ComplianceConst.h"
#include "ComplianceModels.h"
#include "ComplianceNetwork.h"
#include "CompliancePersistence.h"
#include "CompliancePoll.h"
#include "ComplianceState.h"
#include "TapComplianceModule.h"
#include "TapCompliancePanelManager.h"
#include "TapComplianceTracker.h"
#include "UTapComplianceLoadingWidget.h"
#include "Internal/Http/TapHttpCommonParams.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/Guid.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FDesktopComplianceManager& FDesktopComplianceManager::Get()
{
	static FDesktopComplianceManager Instance;
	return Instance;
}

FDesktopComplianceManager::FDesktopComplianceManager() = default;

void FDesktopComplianceManager::EnsureBootstrap()
{
	if (bBootstrapped) return;

	FComplianceState& S = FComplianceState::Get();
	if (S.ClientId.IsEmpty())
	{
		S.ClientId = FTapHttpCommonParams::GetClientID();
		if (S.ClientId.IsEmpty())
		{
			UE_LOG(LogTapCompliance, Warning,
				TEXT("[Manager] ClientId is empty - common params not initialized?"));
		}
	}
	bBootstrapped = true;
}

// =============================================================================
// 选项 / 回调
// =============================================================================

void FDesktopComplianceManager::RegisterCallback(
	TFunction<void(int32, const FString&)> Callback)
{
	EnsureBootstrap();
	FComplianceState::Get().RegisterCallback(MoveTemp(Callback));
}

void FDesktopComplianceManager::UnregisterAllCallbacks()
{
	FComplianceState::Get().UnregisterAllCallbacks();
}

void FDesktopComplianceManager::SetShowSwitchAccount(bool bShow)
{
	FComplianceState::Get().bShowSwitchAccount = bShow;
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Manager] SetShowSwitchAccount=%s"), bShow ? TEXT("true") : TEXT("false"));
}

void FDesktopComplianceManager::SetUseAgeRange(bool bUse)
{
	FComplianceState::Get().bUseAgeRange = bUse;
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Manager] SetUseAgeRange=%s"), bUse ? TEXT("true") : TEXT("false"));
}

bool FDesktopComplianceManager::UsesAgeRange() const
{
	return FComplianceState::Get().bUseAgeRange;
}

// =============================================================================
// 主流程
// =============================================================================

void FDesktopComplianceManager::StartUp(const FString& UserId)
{
	EnsureBootstrap();

	FComplianceState& S = FComplianceState::Get();

	if (bIsStarting)
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Manager] StartUp ignored: another startup is in progress (newUser=%s)"),
			*UserId);
		return;
	}

	if (UserId.IsEmpty())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[Manager] StartUp ignored: empty UserId"));
		DispatchCallback(TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR,
			TEXT("{\"reason\":\"empty user id\"}"));
		return;
	}

	bIsStarting = true;

	// 对齐 Unity ComplianceNewJob.cs:133 ClearUserCache 行为: 每次 StartUp 都视为
	// "用户主动重新开始", 无条件清理上一轮残留 (Poll / 旧 session / 旧面板).
	// 保留 verification 缓存 (bClearCache=false), 避免无谓重新弹实名.
	if (!S.CurrentUserId.IsEmpty())
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Manager] StartUp: cleanup previous session (oldUser=%s) before begin new (newUser=%s)"),
			*S.CurrentUserId, *UserId);
		Logout(/*bClearCache=*/false);
	}

	S.CurrentUserId      = UserId;
	S.bUsedManualVerify  = false;
	// 一次性 GUID, 用于一次 startup 内所有埋点事件的聚合
	// (对齐 Unity ComplianceNewJob.cs:131 Guid.NewGuid)
	S.TrackingSessionId  = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Manager] StartUp begin: UserId=%s TrackId=%s"),
		*UserId, *S.TrackingSessionId);

	// 埋点: startup 开始
	FTapComplianceTracker::Get().TrackStart(TEXT("startup"), S.TrackingSessionId);

	// 对齐 Unity TapTapComplianceManager.cs:124 UIManager.OpenLoading: 启动期间几轮网络请求
	// 累计可能 1-3 秒, 用全屏蒙层提供反馈. 首个业务面板出现 (走 CloseAllPanels) 或 Finish()
	// 时自动收掉.
	if (GEngine && GEngine->GameViewport)
	{
		UTapComplianceLoadingWidget::ShowLoading(GEngine->GameViewport->GetWorld());
	}

	FComplianceAuthFlow::StartUp(UserId);
}

void FDesktopComplianceManager::Logout(bool bClearCache)
{
	FComplianceState& S = FComplianceState::Get();
	const FString UserId = S.CurrentUserId;

	UE_LOG(LogTapCompliance, Log,
		TEXT("[Manager] Logout: UserId=%s clearCache=%s"),
		*UserId, bClearCache ? TEXT("true") : TEXT("false"));

	FCompliancePoll::Get().Logout();

	if (bClearCache && !UserId.IsEmpty())
	{
		FCompliancePersistence::DeleteVerification  (UserId);
		FCompliancePersistence::DeleteUserAntiConfig(UserId);
	}

	S.ResetUserState();
	FTapCompliancePanelManager::Get().CloseAllPanels();
}

// =============================================================================
// 支付
// =============================================================================

void FDesktopComplianceManager::CheckPaymentLimit(
	int64 AmountFen,
	TFunction<void(int32, const FString&, const FString&)> OnDone,
	TFunction<void(const FString&)> OnError)
{
	EnsureBootstrap();

	const FString UserId = FComplianceState::Get().CurrentUserId;
	if (UserId.IsEmpty())
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Manager] CheckPaymentLimit before StartUp - compliance unavailable"));
		if (OnError) OnError(TEXT("TapCompliance is not started"));
		return;
	}

	FComplianceNetwork::CheckPayable(UserId, AmountFen,
		[OnDone = MoveTemp(OnDone), OnError = MoveTemp(OnError)](bool bSuccess, const FPayableResult& Result, const FComplianceError& Err)
		{
			if (!bSuccess)
			{
				// 对齐 Android PaymentApi 的 TOKEN_EXPIRED 处理与 Unity catch (token expired) → Exit():
				// 合规 token 失效不能只静默回失败给单次支付回调, 必须退出合规会话避免游戏继续持有失效态.
				if (Err.IsTokenExpired())
				{
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Manager] CheckPayable: token expired → Logout + EXITED"));
					FDesktopComplianceManager::Get().Logout(/*bClearCache=*/true);
					FDesktopComplianceManager::Get().DispatchCallback(TapComplianceConst::EXITED, FString());
					if (OnError) OnError(Err.ErrorDescription);
					return;
				}
				UE_LOG(LogTapCompliance, Warning,
					TEXT("[Manager] CheckPayable failed: code=%d error=%s"),
					Err.ErrorCode, *Err.Error);
				if (OnError) OnError(Err.ErrorDescription);
				return;
			}

			if (!Result.bAllow
				&& !Result.Title.IsEmpty()
				&& !Result.Content.IsEmpty())
			{
				// 对齐 Android/Unity: 弹窗只是告知, "返回游戏" 由 widget 内部自动 dismiss,
				// 这里两个委托保持空 lambda — 游戏方需要附加处理时再监听.
				FTapCompliancePanelManager::Get().ShowHealthMinorPaymentLimitPanel(
					FText::FromString(Result.Title),    // 服务端 payable.title
					FText::FromString(Result.Content),  // 服务端 payable.description_plain
					FOnHealthMinorPaymentLimitReturnGame::CreateLambda([](){ /* dismiss only, game continues */ }),
					FOnHealthMinorPaymentLimitClosed    ::CreateLambda([](){ /* dismiss only, game continues */ })
				);
			}

			if (OnDone) OnDone(Result.bAllow ? 1 : 0, Result.Title, Result.Content);
		});
}

void FDesktopComplianceManager::SubmitPayment(
	int64 AmountFen,
	TFunction<void(bool, const FString&)> OnDone)
{
	EnsureBootstrap();

	const FString UserId = FComplianceState::Get().CurrentUserId;
	if (UserId.IsEmpty())
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Manager] SubmitPayment before StartUp - skipping report"));
		if (OnDone) OnDone(false, TEXT("TapCompliance is not started"));
		return;
	}

	FComplianceNetwork::SubmitPayment(UserId, AmountFen,
		[OnDone = MoveTemp(OnDone)](bool bSuccess, const FComplianceError& Err)
		{
			if (bSuccess)
			{
				if (OnDone) OnDone(true, FString());
			}
			else
			{
				// 对齐 Android PaymentApi / Unity Exit(): token 失效必须退出合规会话, 不能只回失败给本次回调.
				if (Err.IsTokenExpired())
				{
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Manager] SubmitPayment: token expired → Logout + EXITED"));
					FDesktopComplianceManager::Get().Logout(/*bClearCache=*/true);
					FDesktopComplianceManager::Get().DispatchCallback(TapComplianceConst::EXITED, FString());
					if (OnDone) OnDone(false, TEXT("Token expired"));
					return;
				}
				UE_LOG(LogTapCompliance, Warning,
					TEXT("[Manager] SubmitPayment failed: code=%d error=%s"),
					Err.ErrorCode, *Err.Error);
				if (OnDone) OnDone(false, Err.ErrorDescription);
			}
		});
}

// =============================================================================
// 同步查询
// =============================================================================

int32 FDesktopComplianceManager::GetAgeRange()
{
	return FComplianceState::Get().GetExposedAgeRange();
}

int32 FDesktopComplianceManager::GetUserRemainTime()
{
	const int32 V = FComplianceState::Get().RemainSeconds.GetValue();
	return V < 0 ? 0 : V;
}

// =============================================================================
// 事件分发
// =============================================================================

void FDesktopComplianceManager::DispatchCallback(int32 Code, const FString& ExtraJson)
{
	ClearStartupGuardIfTerminal(Code);

	FString EffectiveExtraJson = ExtraJson;
	if (Code == TapComplianceConst::LOGIN_SUCCESS
		|| Code == TapComplianceConst::EXITED
		|| Code == TapComplianceConst::SWITCH_ACCOUNT)
	{
		const FComplianceState& State = FComplianceState::Get();
		TSharedRef<FJsonObject> UserChanged = MakeShared<FJsonObject>();
		UserChanged->SetStringField(TEXT("anti_addict_user_identifier"),
			Code == TapComplianceConst::LOGIN_SUCCESS ? State.CurrentUserId : FString());
		UserChanged->SetStringField(TEXT("anti_addict_session_id"),
			Code == TapComplianceConst::LOGIN_SUCCESS ? State.CurrentSessionId : FString());

		FString UserChangedJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&UserChangedJson);
		FJsonSerializer::Serialize(UserChanged, Writer);
		EffectiveExtraJson = UserChangedJson;
	}

	FComplianceState::Get().DispatchCallback(Code, EffectiveExtraJson);
}

void FDesktopComplianceManager::ClearStartupGuardIfTerminal(int32 Code)
{
	switch (Code)
	{
	case TapComplianceConst::LOGIN_SUCCESS:
	case TapComplianceConst::EXITED:
	case TapComplianceConst::SWITCH_ACCOUNT:
	case TapComplianceConst::AGE_LIMIT:
	case TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR:
	case TapComplianceConst::REAL_NAME_STOP:
		bIsStarting = false;
		break;
	default:
		break;
	}
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#include "ComplianceAuthFlow.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "ComplianceModels.h"
#include "ComplianceNetwork.h"
#include "CompliancePersistence.h"
#include "CompliancePoll.h"
#include "ComplianceState.h"
#include "DesktopComplianceManager.h"
#include "IdCardValidator.h"
#include "TapComplianceModule.h"
#include "TapCompliancePanelManager.h"
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"
#include "TapComplianceTracker.h"
#include "UTapComplianceLoadingWidget.h"
#include "UTapComplianceToast.h"
#include "Async/Async.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformTime.h"
#include "Internal/Http/TapHttpTime.h"
#include "Kismet/KismetSystemLibrary.h"

namespace
{
	void DispatchOnGameThread(int32 Code, const FString& ExtraJson)
	{
		FDesktopComplianceManager::Get().DispatchCallback(Code, ExtraJson);
	}

	bool ParseHHMM(const FString& In, int32& OutH, int32& OutM)
	{
		FString H, M;
		if (!In.Split(TEXT(":"), &H, &M)) return false;
		OutH = FCString::Atoi(*H);
		OutM = FCString::Atoi(*M);
		return OutH >= 0 && OutH <= 23 && OutM >= 0 && OutM <= 59;
	}

	bool HasRequiredComplianceScope(const TArray<FString>& Scopes)
	{
		auto HasScope = [&Scopes](const FString& Scope)
		{
			for (const FString& S : Scopes)
			{
				if (S.Equals(Scope))
				{
					return true;
				}
			}
			return false;
		};

		if (FComplianceState::Get().bUseAgeRange)
		{
			return HasScope(TapComplianceConst::SCOPE_COMPLIANCE);
		}

		return HasScope(TapComplianceConst::SCOPE_COMPLIANCE)
			|| HasScope(TapComplianceConst::SCOPE_COMPLIANCE_BASIC);
	}

	bool TapLoginAccountHasRequiredComplianceScope(const FTapAccount& LoginAccount)
	{
		return HasRequiredComplianceScope(LoginAccount.AccessToken.Scopes);
	}

	bool TapLoginAccessTokenHasRequiredComplianceScope(const FTapAccessToken& Token)
	{
		return HasRequiredComplianceScope(Token.Scopes);
	}

	FDateTime FromUnixTimeLocal(int64 UnixSeconds)
	{
		const FDateTime Utc = FDateTime::FromUnixTimestamp(UnixSeconds);
		const FTimespan LocalOffset = FDateTime::Now() - FDateTime::UtcNow();
		return Utc + LocalOffset;
	}

	FPlayableResult ComputeOfflinePlayableAtInternal(const FDateTime& Now)
	{
		FPlayableResult Out;
		const FComplianceState& S = FComplianceState::Get();

		if (S.Verification.CheckIsAdult())
		{
			Out.RemainTime = TapComplianceConst::ADULT_INFINITE_REMAIN_TIME;
			return Out;
		}

		// 未成年: 仅在 holidays 列表内的日期 + timeStart..timeEnd 区间可玩
		const FTimeRangeConfig& TR = S.UserAntiConfig.LocalConfig.TimeRangeConfig;
		if (TR.Holidays.Num() == 0 || TR.TimeStart.IsEmpty() || TR.TimeEnd.IsEmpty())
		{
			Out.RemainTime = 0;
			return Out;
		}

		const FString TodayStr = Now.ToString(TEXT("%Y-%m-%d"));
		bool bIsHoliday = false;
		for (const FString& H : TR.Holidays)
		{
			if (H.Equals(TodayStr))
			{
				bIsHoliday = true;
				break;
			}
		}
		if (!bIsHoliday)
		{
			Out.RemainTime = 0;
			return Out;
		}

		int32 SH=0, SM=0, EH=0, EM=0;
		if (!ParseHHMM(TR.TimeStart, SH, SM) || !ParseHHMM(TR.TimeEnd, EH, EM))
		{
			Out.RemainTime = 0;
			return Out;
		}

		const FDateTime TodayStart(Now.GetYear(), Now.GetMonth(), Now.GetDay(), SH, SM, 0);
		const FDateTime TodayEnd  (Now.GetYear(), Now.GetMonth(), Now.GetDay(), EH, EM, 0);

		if (Now < TodayStart || Now >= TodayEnd)
		{
			Out.RemainTime = 0;
			return Out;
		}

		const FTimespan Remain = TodayEnd - Now;
		const double Sec = Remain.GetTotalSeconds();
		Out.RemainTime = Sec > 0.0 ? static_cast<int32>(Sec) : 0;
		return Out;
	}
}

// =============================================================================
// 入口
// =============================================================================

void FComplianceAuthFlow::StartUp(const FString& UserId)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Start: UserId=%s"), *UserId);

	// 对齐 Android ConfigApi.fetchRealNameConfig (ConfigApi.kt:32-42) 与
	// Unity Config.Fetch (Config.cs:38-78) 的缓存兜底策略:
	//   - 成功     → 写入机器绑定缓存 + 继续 Step1
	//   - 网络/5xx → 尝试读机器绑定缓存, 命中则降级继续, 未命中硬失败
	//   - 4xx 业务错 → 缓存救不了 (配置/鉴权问题), 直接硬失败
	//
	// 注: 本地时钟偏差导致的 invalid_time 已由 TapBasic RequestWithSigning 自动 FixTime+重试,
	// 业务侧不会再看到 invalid_time.
	FComplianceNetwork::FetchGlobalConfig(UserId,
		[UserId](bool bOk, const FRealNameConfigResult& Result, const FComplianceError& Err)
		{
			if (bOk)
			{
				FComplianceState::Get().RealNameConfig = Result;
				FCompliancePersistence::SaveRealNameConfig(Result);
				UE_LOG(LogTapCompliance, Log,
					TEXT("[Flow] StartUp: GlobalConfig OK, useManual=%s (cached)"),
					Result.bUseManual ? TEXT("true") : TEXT("false"));
				Step1_FetchVerification(UserId);
				return;
			}

			{
				const FString Detail = Err.RawResponse.IsEmpty()
					? FString::Printf(TEXT("error=%s desc=%s code=%d HTTP=%d"),
						*Err.Error, *Err.ErrorDescription, Err.ErrorCode, Err.HttpStatus)
					: Err.RawResponse;
				UE_LOG(LogTapCompliance, Warning,
					TEXT("[Flow] StartUp: FetchGlobalConfig failed. %s"), *Detail);
			}

			// 分类: 仅网络/5xx/解析错误尝试缓存; 4xx 业务错与 token 失效硬失败
			// (对齐 Android isServerError()||isNoHostError() 语义).
			const bool bRecoverable = Err.IsRecoverableForGlobalConfigCacheFallback();

			if (bRecoverable)
			{
				FRealNameConfigResult Cached;
				if (FCompliancePersistence::LoadRealNameConfig(Cached))
				{
					FComplianceState::Get().RealNameConfig = Cached;
					UE_LOG(LogTapCompliance, Log,
						TEXT("[Flow] StartUp: GlobalConfig fetch failed → using cached config (useManual=%s)"),
						Cached.bUseManual ? TEXT("true") : TEXT("false"));
					Step1_FetchVerification(UserId);
					return;
				}
				UE_LOG(LogTapCompliance, Warning,
					TEXT("[Flow] StartUp: no cached GlobalConfig available, aborting"));
			}

			Finish(TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR);
		});
}

void FComplianceAuthFlow::Finish(int32 Code, const FString& ExtraJson)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Finish: Code=%d"), Code);
	FComplianceState& S = FComplianceState::Get();
	// 兜底收 Loading: 大多数 Finish 路径之前已通过 CloseAllPanels (Show*Panel) 收过,
	// 但纯网络错走 Finish(INVALID_CLIENT_OR_NETWORK_ERROR) 等路径不弹任何面板,
	// 也要把 Loading 蒙层收掉, 避免残留.
	UTapComplianceLoadingWidget::HideLoading();

	// 埋点 (对齐 Unity ComplianceNewJob.cs:131-145 + Android TapComplianceInternal.kt:574,606,639):
	//   LOGIN_SUCCESS / AGE_LIMIT / PERIOD_RESTRICT → TrackSuccess (附 identity_verify_type)
	//   INVALID_CLIENT_OR_NETWORK_ERROR / EXITED    → TrackFailure
	//   REAL_NAME_STOP                              → TrackCancel
	const FString& TrackId = S.TrackingSessionId;
	if (!TrackId.IsEmpty())
	{
		TMap<FString, FString> Extra;
		Extra.Add(TEXT("identity_verify_type"),
			S.bUsedManualVerify ? TEXT("manual_verify") : TEXT("fast_verify"));

		const FTapComplianceTracker& T = FTapComplianceTracker::Get();
		switch (Code)
		{
		case TapComplianceConst::LOGIN_SUCCESS:
		case TapComplianceConst::AGE_LIMIT:
		case TapComplianceConst::PERIOD_RESTRICT:
			T.TrackSuccess(TEXT("startup"), TrackId, Extra);
			break;
		case TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR:
			T.TrackFailure(TEXT("startup"), TrackId,
				FString::FromInt(Code), TEXT("invalid client or network error"), Extra);
			break;
		case TapComplianceConst::EXITED:
			T.TrackFailure(TEXT("startup"), TrackId,
				FString::FromInt(Code), TEXT("token expired"), Extra);
			break;
		case TapComplianceConst::REAL_NAME_STOP:
			T.TrackCancel(TEXT("startup"), TrackId, Extra);
			break;
		default:
			// 其它码 (如 SWITCH_ACCOUNT 由外层 DispatchOnGameThread 直接发) 不在 startup 埋点链路内
			break;
		}
	}

	if (Code == TapComplianceConst::LOGIN_SUCCESS)
	{
		ShowFirstPassTipOnce(S.CurrentUserId);
	}

	DispatchOnGameThread(Code, ExtraJson);
}

void FComplianceAuthFlow::ShowFirstPassTipOnce(const FString& UserId)
{
	if (!IsInGameThread())
	{
		AsyncTask(ENamedThreads::GameThread, [UserId]()
		{
			FComplianceAuthFlow::ShowFirstPassTipOnce(UserId);
		});
		return;
	}

	if (UserId.IsEmpty() || FCompliancePersistence::HasShownFirstPassTip(UserId))
	{
		return;
	}

	if (GEngine && GEngine->GameViewport)
	{
		UTapComplianceToast::ShowToast(
			GEngine->GameViewport->GetWorld(),
			TEXT("已通过防沉迷校验，祝您游戏愉快！"));
	}

	FCompliancePersistence::MarkFirstPassTipShown(UserId);
}

// =============================================================================
// Step 1: FetchVerification
// =============================================================================

void FComplianceAuthFlow::Step1_FetchVerification(const FString& UserId)
{
	// Tier 1: 本地缓存命中 → 直接进 Step 2
	FComplianceVerification Cached;
	if (FCompliancePersistence::LoadVerification(UserId, Cached) && Cached.IsVerified())
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Flow] Step1: cache hit (verified)"));
		FComplianceState::Get().Verification = Cached;
		Step2_ValidateUserConfig(UserId);
		return;
	}

	// Tier 2: TapLogin 当前 token 已含 compliance scope → 直接 FastAuth (不弹 UI)
	if (TapLoginHasComplianceScope(UserId))
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Flow] Step1: TapToken has compliance scope → silent FastAuth"));
		TryFetchByTapToken(UserId, [UserId]()
		{
			// 对齐 Android/Unity: 静默 TapToken 获取失败不再回落到普通 userId 查询,
			// 而是进入快速实名引导, 用户确认后重新授权并再调 TapToken 端点.
			UE_LOG(LogTapCompliance, Log,
				TEXT("[Flow] Step1: silent FastAuth failed, show FastAuth prompt"));
			ShowFastAuthThenManual(UserId);
		});
		return;
	}

	// Tier 3: 服务端查询 (无 TapToken)
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] Step1: cache miss + no compliance scope, query server"));
	FComplianceNetwork::FetchVerification(UserId,
		[UserId](bool bOk, const FComplianceVerification& Result, const FComplianceError& Err)
		{
			OnVerificationFromServer(UserId, bOk, Result, Err);
		});
}

bool FComplianceAuthFlow::TapLoginHasComplianceScope(const FString& UserId)
{
	const TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
	if (!Account.IsSet())
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Flow] Step1: GetCurrentAccount returned empty (userId=%s); skip silent FastAuth and query server"),
			*UserId);
		return false;
	}

	const FTapAccount& LoginAccount = Account.GetValue();
	const TArray<FString>& Scopes = LoginAccount.AccessToken.Scopes;
	auto LogNoScope = [&UserId, &LoginAccount, &Scopes](bool bUseAge)
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Flow] Step1: current TapLogin account has no compliance scope (userId=%s openId=%s unionId=%s scopes=%s useAgeRange=%s); query server"),
			*UserId,
			*LoginAccount.OpenID,
			*LoginAccount.UnionID,
			*FString::Join(Scopes, TEXT(",")),
			bUseAge ? TEXT("true") : TEXT("false"));
	};

	const bool bUseAge = FComplianceState::Get().bUseAgeRange;
	const bool bHasScope = TapLoginAccountHasRequiredComplianceScope(LoginAccount);
	if (!bHasScope)
	{
		LogNoScope(bUseAge);
	}
	return bHasScope;
}

void FComplianceAuthFlow::TryFetchByTapToken(const FString& UserId, TFunction<void()> OnFail)
{
	FComplianceNetwork::FetchVerificationByTapToken(UserId,
		[UserId, OnFail = MoveTemp(OnFail)](bool bOk, const FComplianceVerification& Result, const FComplianceError& Err)
		{
			if (!bOk)
			{
				{
					const FString Detail = Err.RawResponse.IsEmpty()
						? FString::Printf(TEXT("error=%s desc=%s code=%d HTTP=%d"),
							*Err.Error, *Err.ErrorDescription, Err.ErrorCode, Err.HttpStatus)
						: Err.RawResponse;
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Flow] FastAuth FetchByTapToken failed. %s"), *Detail);
				}
				if (OnFail) OnFail();
				return;
			}

			if (Result.IsVerified())
			{
				FComplianceState::Get().Verification = Result;
				FCompliancePersistence::SaveVerification(UserId, Result);
				UE_LOG(LogTapCompliance, Log, TEXT("[Flow] FastAuth pass → Step 2"));
				Step2_ValidateUserConfig(UserId);
				return;
			}

			if (Result.IsWaiting())
			{
				UE_LOG(LogTapCompliance, Log, TEXT("[Flow] FastAuth waiting → AuthReviewing(Waiting) + REAL_NAME_STOP"));
				FTapCompliancePanelManager::Get().ShowAuthReviewingPanel(
					EAuthReviewingState::Waiting,
					FOnAuthReviewingConfirmed::CreateLambda([](){}),
					FOnAuthReviewingClosed   ::CreateLambda([](){})
				);
				Finish(TapComplianceConst::REAL_NAME_STOP);
				return;
			}

			UE_LOG(LogTapCompliance, Log,
				TEXT("[Flow] FastAuth status=%s → fall back"), *Result.Status);
			if (OnFail) OnFail();
		});
}

void FComplianceAuthFlow::TryFetchByTapToken(
	const FString& UserId,
	const FTapAccessToken& Token,
	TFunction<void()> OnFail)
{
	FComplianceNetwork::FetchVerificationByTapToken(UserId, Token,
		[UserId, OnFail = MoveTemp(OnFail)](bool bOk, const FComplianceVerification& Result, const FComplianceError& Err) mutable
		{
			OnTapTokenVerificationFetched(UserId, MoveTemp(OnFail), bOk, Result, Err);
		});
}

void FComplianceAuthFlow::OnTapTokenVerificationFetched(
	const FString& UserId,
	TFunction<void()> OnFail,
	bool bOk,
	const FComplianceVerification& Result,
	const FComplianceError& Err)
{
	if (!bOk)
	{
		const FString Detail = Err.RawResponse.IsEmpty()
			? FString::Printf(TEXT("error=%s desc=%s code=%d HTTP=%d"),
				*Err.Error, *Err.ErrorDescription, Err.ErrorCode, Err.HttpStatus)
			: Err.RawResponse;
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Flow] FastAuth FetchByTapToken failed. %s"), *Detail);
		if (OnFail) OnFail();
		return;
	}

	if (Result.IsVerified())
	{
		FComplianceState::Get().Verification = Result;
		FCompliancePersistence::SaveVerification(UserId, Result);
		UE_LOG(LogTapCompliance, Log, TEXT("[Flow] FastAuth pass -> Step 2"));
		Step2_ValidateUserConfig(UserId);
		return;
	}

	if (Result.IsWaiting())
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[Flow] FastAuth waiting -> AuthReviewing(Waiting) + REAL_NAME_STOP"));
		FTapCompliancePanelManager::Get().ShowAuthReviewingPanel(
			EAuthReviewingState::Waiting,
			FOnAuthReviewingConfirmed::CreateLambda([](){}),
			FOnAuthReviewingClosed   ::CreateLambda([](){})
		);
		Finish(TapComplianceConst::REAL_NAME_STOP);
		return;
	}

	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] FastAuth status=%s -> fall back"), *Result.Status);
	if (OnFail) OnFail();
}

void FComplianceAuthFlow::OnVerificationFromServer(
	const FString& UserId, bool bOk,
	const FComplianceVerification& Result,
	const FComplianceError& Err)
{
	if (!bOk)
	{
		// 网络层错误：可能是首次实名前的预期 4xx (failed/not-found)，也可能是真异常。
		// 简单策略：直接 fall through 到手动实名 UI，让用户主动认证。
		{
			const FString Detail = Err.RawResponse.IsEmpty()
				? FString::Printf(TEXT("error=%s desc=%s code=%d HTTP=%d"),
					*Err.Error, *Err.ErrorDescription, Err.ErrorCode, Err.HttpStatus)
				: Err.RawResponse;
			UE_LOG(LogTapCompliance, Warning,
				TEXT("[Flow] Step1: server query failed; show FastAuth prompt before manual fallback. %s"), *Detail);
		}
		ShowFastAuthThenManual(UserId);
		return;
	}

	if (Result.IsVerified())
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Step1: server pass"));
		FComplianceState::Get().Verification = Result;
		FCompliancePersistence::SaveVerification(UserId, Result);
		Step2_ValidateUserConfig(UserId);
		return;
	}

	if (Result.IsWaiting())
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Step1: server waiting → AuthReviewing(Waiting) + REAL_NAME_STOP"));
		FTapCompliancePanelManager::Get().ShowAuthReviewingPanel(
			EAuthReviewingState::Waiting,
			FOnAuthReviewingConfirmed::CreateLambda([](){}),
			FOnAuthReviewingClosed   ::CreateLambda([](){})
		);
		Finish(TapComplianceConst::REAL_NAME_STOP);
		return;
	}

	// "failed" 或未知 status → 走手动实名
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] Step1: server result not verified (status=%s); show FastAuth prompt before manual fallback"),
		*Result.Status);
	ShowFastAuthThenManual(UserId);
}

void FComplianceAuthFlow::ShowFastAuthThenManual(const FString& UserId)
{
	// FastAuth 引导面板：
	//   "去使用" (Confirmed)  → 走 TapLogin OAuth 升级 scope，再调 FastAuth 端点
	//   "不使用" (Cancelled)  → 直接手动实名 (仅允许手动且非"Tap 已登录但缺 compliance scope"时显示)
	//   关闭按钮 (Closed)     → REAL_NAME_STOP(9002)
	const bool bShowCancel = ShouldShowFastAuthCancelButton();

	FTapCompliancePanelManager::Get().ShowFastAuthPanel(
		/*bShowCancelButton=*/ bShowCancel,
		FOnFastAuthConfirmed::CreateLambda([UserId](){ OnFastAuthAccept(UserId); }),
		FOnFastAuthCancelled::CreateLambda([UserId](){ ShowRealNameManual(UserId); }),
		FOnFastAuthClosed   ::CreateLambda([](){ Finish(TapComplianceConst::REAL_NAME_STOP); })
	);
}

bool FComplianceAuthFlow::ShouldShowFastAuthCancelButton()
{
	// 对齐 Android TapComplianceInternal.goTapFastAuth + TapAuthorizationDialog.isManualIdentify:
	//   isTapUser = TapLogin 已登录 && 当前 token 不含 compliance/compliance_basic scope
	//   useManual = manual_auth_enable == true
	//   显示"不使用" = !isTapUser && useManual
	// 也等价于 Unity ComplianceWorker.cs:396:
	//   justShowConfirmBtn = isTapUser && !haveCompliance || !Config.Current.useManual
	const FComplianceState& State = FComplianceState::Get();
	const bool bUseManual = State.RealNameConfig.bUseManual;
	bool bTapLoggedIn = false;
	bool bHasComplianceScope = false;

	const TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
	if (Account.IsSet())
	{
		bTapLoggedIn = true;
		bHasComplianceScope = TapLoginAccountHasRequiredComplianceScope(Account.GetValue());
	}

	const bool bIsTapUserWithoutCompliance = bTapLoggedIn && !bHasComplianceScope;
	const bool bShowCancel = bUseManual && !bIsTapUserWithoutCompliance;
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] FastAuth prompt button policy: showCancel=%s useManual=%s tapLoggedIn=%s hasComplianceScope=%s"),
		bShowCancel ? TEXT("true") : TEXT("false"),
		bUseManual ? TEXT("true") : TEXT("false"),
		bTapLoggedIn ? TEXT("true") : TEXT("false"),
		bHasComplianceScope ? TEXT("true") : TEXT("false"));

	return bShowCancel;
}

void FComplianceAuthFlow::OnFastAuthAccept(const FString& UserId)
{
	const bool bUseAge = FComplianceState::Get().bUseAgeRange;
	const ETapLoginScope Scope = bUseAge
		? ETapLoginScope::Compliance
		: ETapLoginScope::ComplianceBasic;

	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] FastAuth accept -> TapLogin token-only authorize scope=%s"),
		bUseAge ? TEXT("compliance") : TEXT("compliance_basic"));

	FTapLoginInternal::AuthorizeForAccessToken(
		TArray<ETapLoginScope>{ Scope },
		[UserId](const FTapAccessTokenResult& Result)
		{
			if (Result.bSuccess)
			{
				if (!TapLoginAccessTokenHasRequiredComplianceScope(Result.Token))
				{
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Flow] TapLogin token-only authorize returned token without required compliance scope (scopes=%s) -> manual"),
						*FString::Join(Result.Token.Scopes, TEXT(",")));
					ShowRealNameManual(UserId);
					return;
				}

				UE_LOG(LogTapCompliance, Log,
					TEXT("[Flow] TapLogin token-only authorize success -> retry FastAuth"));
				TryFetchByTapToken(UserId, Result.Token, [UserId]()
				{
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Flow] FastAuth still failed after token-only authorize -> manual"));
					ShowRealNameManual(UserId);
				});
				return;
			}

			if (Result.bCancelled)
			{
				// Align Unity: LOGIN_CANCEL re-prompts FastAuth instead of going directly to manual.
				UE_LOG(LogTapCompliance, Log,
					TEXT("[Flow] TapLogin token-only authorize cancelled -> re-prompt FastAuth"));
				ShowFastAuthThenManual(UserId);
				return;
			}

			UE_LOG(LogTapCompliance, Warning,
				TEXT("[Flow] TapLogin token-only authorize failed (code=%d msg=%s) -> manual"),
				Result.ErrorCode, *Result.ErrorMessage);
			ShowRealNameManual(UserId);
		});

}

void FComplianceAuthFlow::ShowRealNameManual(const FString& UserId)
{
	// 标记本次 startup 走过手动实名 → 埋点 identity_verify_type=manual_verify
	// (对齐 Android TapComplianceInternal.kt:341 isManuallyVerify)
	FComplianceState::Get().bUsedManualVerify = true;

	FTapCompliancePanelManager::Get().ShowPanel(
		FOnCompliancePanelClose::CreateLambda([](){
			Finish(TapComplianceConst::REAL_NAME_STOP);
		}),
		FOnCompliancePanelBack::CreateLambda([UserId](){
			// 返回 → 重新走 FastAuth 引导
			ShowFastAuthThenManual(UserId);
		}),
		FOnCompliancePanelSubmit::CreateLambda([UserId](const FString& Name, const FString& IdCard){
			OnRealNameSubmit(UserId, Name, IdCard);
		})
	);
}

void FComplianceAuthFlow::OnRealNameSubmit(
	const FString& UserId, const FString& Name, const FString& IdCard)
{
	const FString TrimmedName   = Name.TrimStartAndEnd();
	const FString TrimmedIdCard = IdCard.TrimStartAndEnd();

	// 客户端预校验
	if (TrimmedName.IsEmpty())
	{
		FTapCompliancePanelManager::Get().OnSubmitFailed(TEXT("请输入姓名"));
		return;
	}
	if (!FIdCardValidator::IsValid(TrimmedIdCard))
	{
		FTapCompliancePanelManager::Get().OnSubmitFailed(TEXT("身份证号格式不正确"));
		return;
	}

	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] Manual submit: name=%s id=***"), *TrimmedName);

	FComplianceNetwork::FetchVerificationManual(UserId, TrimmedName, TrimmedIdCard,
		[UserId](bool bOk, const FComplianceVerification& Result, const FComplianceError& Err)
		{
			if (!bOk)
			{
				// 选择最适合展示给用户的文案, 优先级:
				//   1) Err.Msg                 ← 服务端 data.msg, 简洁中文 (例 "输入信息有误", "实名信息不存在")
				//   2) ErrorDescription 中 "desc = " 之后的内容  ← gRPC 嵌套错误里的最内层 message
				//   3) Err.Error               ← 业务 error code (例 "business_code_error")
				//   4) 默认 "提交失败，请检查网络后重试"
				FString Msg = Err.Msg;
				if (Msg.IsEmpty() && !Err.ErrorDescription.IsEmpty())
				{
					// 例: "tds-tapsdk.tapapis.com.APIS_ERROR: InvalidArgument: rpc error: code = InvalidArgument desc = 输入信息有误"
					// 取最后一个 "desc = " 后面的内容
					const FString Marker = TEXT("desc = ");
					const int32 Pos = Err.ErrorDescription.Find(
						Marker, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
					if (Pos != INDEX_NONE)
					{
						Msg = Err.ErrorDescription.Mid(Pos + Marker.Len()).TrimStartAndEnd();
					}
					else
					{
						Msg = Err.ErrorDescription;
					}
				}
				if (Msg.IsEmpty())
				{
					Msg = Err.Error.IsEmpty() ? TEXT("提交失败，请检查网络后重试") : Err.Error;
				}
				FTapCompliancePanelManager::Get().OnSubmitFailed(Msg);
				return;
			}

			if (Result.IsWaiting())
			{
				// 提交后审核中 → AuthReviewing(Waiting) 标准弹窗 (替代旧 RealName Toast 复用)
				UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Submit waiting → AuthReviewing(Waiting) + REAL_NAME_STOP"));
				FCompliancePersistence::SaveVerification(UserId, Result);
				FComplianceState::Get().Verification = Result;
				FTapCompliancePanelManager::Get().ShowAuthReviewingPanel(
					EAuthReviewingState::Waiting,
					FOnAuthReviewingConfirmed::CreateLambda([](){}),
					FOnAuthReviewingClosed   ::CreateLambda([](){})
				);
				Finish(TapComplianceConst::REAL_NAME_STOP);
				return;
			}

			if (!Result.IsVerified())
			{
				// 提交后服务端判定失败 → AuthReviewing(Failed) 弹窗, OnConfirmed 重开 RealName 让用户重填
				// (对齐 Android RealNameVerifyDialog 的 else 分支语义, 但 UE5 用独立弹窗代替 inline toast)
				UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Submit failed (status=%s) → AuthReviewing(Failed)"), *Result.Status);
				// 顺带弹一次错误 Toast 增强反馈.
				if (GEngine && GEngine->GameViewport)
				{
					UTapComplianceToast::ShowToast(
						GEngine->GameViewport->GetWorld(),
						TEXT("认证未通过，请提交真实信息"));
				}
				FTapCompliancePanelManager::Get().ShowAuthReviewingPanel(
					EAuthReviewingState::Failed,
					FOnAuthReviewingConfirmed::CreateLambda([UserId](){
						// 重新展示手动实名输入面板, 传 bHasAuthRecord=true 使 RealName 内部
						// 文案也切到 auth_failed (与 Android RealNameVerifyDialog
						// hasAuthRecord=true 行为对齐). 复用 ShowRealNameManual 内部的回调闭包.
						FTapCompliancePanelManager::Get().ShowPanel(
							FOnCompliancePanelClose::CreateLambda([](){
								Finish(TapComplianceConst::REAL_NAME_STOP);
							}),
							FOnCompliancePanelBack::CreateLambda([UserId](){
								ShowFastAuthThenManual(UserId);
							}),
							FOnCompliancePanelSubmit::CreateLambda([UserId](const FString& Name, const FString& IdCard){
								OnRealNameSubmit(UserId, Name, IdCard);
							}),
							/*bHasAuthRecord=*/true);
					}),
					FOnAuthReviewingClosed::CreateLambda([](){ Finish(TapComplianceConst::REAL_NAME_STOP); })
				);
				return;
			}

			FComplianceState::Get().Verification = Result;
			FCompliancePersistence::SaveVerification(UserId, Result);
			FTapCompliancePanelManager::Get().OnSubmitSuccess();
			FTapCompliancePanelManager::Get().CloseAllPanels();

			// 对齐 Unity TaptapComplianceIDInputController.cs:138 OpenToast("提交成功", Success):
			// 手动实名成功后弹一次成功 toast, 让用户有明确反馈.
			// 使用统一 Slate Toast，避免依赖额外 WBP 资产.
			if (GEngine && GEngine->GameViewport)
			{
				UTapComplianceToast::ShowToast(
					GEngine->GameViewport->GetWorld(),
					TEXT("提交成功"));
			}

			Step2_ValidateUserConfig(UserId);
		});
}

// =============================================================================
// Step 2: ValidateUserConfig
// =============================================================================

void FComplianceAuthFlow::Step2_ValidateUserConfig(const FString& UserId)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Step2: fetch user anti-config"));

	FComplianceNetwork::FetchUserAntiConfig(UserId,
		[UserId](bool bOk, const FUserAntiConfig& Result, const FComplianceError& Err)
		{
			FUserAntiConfig EffectiveCfg;

			if (bOk)
			{
				EffectiveCfg = Result;
				FCompliancePersistence::SaveUserAntiConfig(UserId, Result);
			}
			else if (Err.IsTokenExpired())
			{
				UE_LOG(LogTapCompliance, Warning,
					TEXT("[Flow] Step2: token expired → EXITED"));
				FDesktopComplianceManager::Get().Logout(/*bClearCache=*/true);
				Finish(TapComplianceConst::EXITED);
				return;
			}
			else
			{
				if (!Err.IsRecoverableForCacheFallback())
				{
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Flow] Step2: get-config-by-token hard failed (HTTP=%d code=%d error=%s); INVALID_CLIENT_OR_NETWORK_ERROR"),
						Err.HttpStatus, Err.ErrorCode, *Err.Error);
					Finish(TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR);
					return;
				}

				FUserAntiConfig Cached;
				if (FCompliancePersistence::LoadUserAntiConfig(UserId, Cached))
				{
					UE_LOG(LogTapCompliance, Log,
						TEXT("[Flow] Step2: recoverable server/network failure -> using cached config"));
					FComplianceState::Get().ResetOfflineLocalTimeIfNeeded();
					EffectiveCfg = Cached;
				}
				else
				{
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Flow] Step2: server failed + no cache → INVALID_CLIENT_OR_NETWORK_ERROR"));
					Finish(TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR);
					return;
				}
			}

			FComplianceState& S = FComplianceState::Get();
			S.UserAntiConfig          = EffectiveCfg;
			S.CurrentSessionId        = FComplianceState::GenerateSession32();

			// 对齐 Unity Verification.setAgeState (Verification.cs:236) 和 Android TapComplianceInternal.fetchUserAntiConfig
			// (TapComplianceInternal.kt:494): 服务端 age_limit<0 时, 用 is_adult 映射为内部哨兵
			//   - is_adult=true  → AGE_LIMIT_SENTINEL_ADULT(110)
			//   - is_adult=false → AGE_LIMIT_SENTINEL_MINOR(100)
			// 否则直接使用服务端下发的 age_limit. 这样 CheckIsAdult() 才能正确识别 useAgeRange=false 下的成年人.
			const int32 RawAge = EffectiveCfg.UserState.AgeLimit;
			const bool  bAdult = EffectiveCfg.UserState.bIsAdult;
			S.Verification.AgeLimit = (RawAge < 0)
				? (bAdult ? TapComplianceConst::AGE_LIMIT_SENTINEL_ADULT
				          : TapComplianceConst::AGE_LIMIT_SENTINEL_MINOR)
				: RawAge;
			S.Verification.bIsAdult = bAdult;
			FCompliancePersistence::SaveVerification(UserId, S.Verification);

			UE_LOG(LogTapCompliance, Log,
				TEXT("[Flow] Step2: AgeLimit=%d isAdult=%s allow=%s policy.active=%s heartbeat=%d"),
				EffectiveCfg.UserState.AgeLimit,
				EffectiveCfg.UserState.bIsAdult ? TEXT("true") : TEXT("false"),
				EffectiveCfg.AgeCheckResult.bAllow ? TEXT("true") : TEXT("false"),
				*EffectiveCfg.Policy.Active,
				EffectiveCfg.Policy.HeartbeatInterval);

			if (!EffectiveCfg.AgeCheckResult.bAllow)
			{
				Finish(TapComplianceConst::AGE_LIMIT);
				return;
			}

			Step3_ValidatePlayable(UserId);
		});
}

// =============================================================================
// Step 3: ValidatePlayable
// =============================================================================

bool FComplianceAuthFlow::ShouldUseLocalOnly()
{
	const FAntiPolicy& P = FComplianceState::Get().UserAntiConfig.Policy;
	return P.Model.Equals(TapComplianceConst::POLICY_MODEL_LOCAL)
		&& P.Active.Equals(TapComplianceConst::POLICY_ACTIVE_TIME_RANGE);
}

void FComplianceAuthFlow::Step3_ValidatePlayable(const FString& UserId)
{
	// 对齐 Android PlayLogModel.checkUserState (PlayLogModel.kt:24): policy_model=local +
	// active=time_range 时, 服务端要求本地判断, 不再发心跳, 避免误把暂时离线的本地策略玩家
	// 当成网络错误.
	if (ShouldUseLocalOnly())
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Flow] Step3: policy_model=local + time_range → skip heartbeat, use local calc"));
		HandleStep3Result(UserId, ComputeOfflinePlayable());
		return;
	}

	UE_LOG(LogTapCompliance, Log, TEXT("[Flow] Step3: heartbeat"));

	const FString SessionId = FComplianceState::Get().CurrentSessionId;
	FComplianceNetwork::Heartbeat(UserId, SessionId,
		[UserId](bool bOk, const FPlayableResult& Result, const FComplianceError& Err)
		{
			FComplianceState& S = FComplianceState::Get();

			FPlayableResult Effective;
			if (bOk)
			{
				Effective = Result;
			}
			else if (Err.IsTokenExpired())
			{
				FDesktopComplianceManager::Get().Logout(/*bClearCache=*/true);
				Finish(TapComplianceConst::EXITED);
				return;
			}
			else if (S.UserAntiConfig.Policy.Active.Equals(TapComplianceConst::POLICY_ACTIVE_TIME_RANGE))
			{
				UE_LOG(LogTapCompliance, Log,
					TEXT("[Flow] Step3: heartbeat failed → use offline calc"));
				Effective = ComputeOfflinePlayable();
			}
			else
			{
				UE_LOG(LogTapCompliance, Warning,
					TEXT("[Flow] Step3: heartbeat failed and no offline fallback → INVALID_CLIENT_OR_NETWORK_ERROR"));
				Finish(TapComplianceConst::INVALID_CLIENT_OR_NETWORK_ERROR);
				return;
			}

			HandleStep3Result(UserId, Effective);
		});
}

void FComplianceAuthFlow::HandleStep3Result(const FString& UserId, const FPlayableResult& Effective)
{
	FComplianceState& S = FComplianceState::Get();
	S.CurrentPlayableResult = Effective;
	S.RemainSeconds.Set(FMath::Max(0, Effective.RemainTime));

	const int32 HeartbeatInterval = S.UserAntiConfig.Policy.HeartbeatInterval;

	// 成年人 → 启动 Poll → LOGIN_SUCCESS
	if (S.Verification.CheckIsAdult())
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Flow] Step3: adult → LOGIN_SUCCESS"));
		FCompliancePoll::Get().StartUp(HeartbeatInterval);
		Finish(TapComplianceConst::LOGIN_SUCCESS);
		return;
	}

	// 未成年
	const bool bAllowEnter = (Effective.RemainTime > 0);
	const double PlayableCheckedAtSeconds = FPlatformTime::Seconds();
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] Step3: minor, RemainTime=%d, bAllowEnter=%s"),
		Effective.RemainTime, bAllowEnter ? TEXT("true") : TEXT("false"));

	// "切换账号" 按钮: 仅在禁玩态 + bShowSwitchAccount=true 时绑定
	// (与 Unity OnCheckedPlayableWithMinorAsync 一致)
	FOnHealthMinorTimeLimitSwitchAccount OnSwitchAccountCb;
	if (!bAllowEnter && S.bShowSwitchAccount)
	{
		OnSwitchAccountCb = FOnHealthMinorTimeLimitSwitchAccount::CreateLambda([]()
		{
			FDesktopComplianceManager::Get().Logout(/*bClearCache=*/false);
			DispatchOnGameThread(TapComplianceConst::SWITCH_ACCOUNT, FString());
		});
	}

	// 动态文案: 优先 heartbeat 返回, 空时回退本地 UITipText, 含 "# ${remaining} #" 占位符则替换
	FText ReminderTitle, ReminderContent;
	ResolveHealthReminderText(Effective, S.UserAntiConfig, ReminderTitle, ReminderContent);

	FTapCompliancePanelManager::Get().ShowHealthMinorTimeLimitPanel(
		/*bAllowEnterGame=*/bAllowEnter,
		ReminderTitle,
		ReminderContent,
		FOnHealthMinorTimeLimitExitGame::CreateLambda(
			[bAllowEnter, HeartbeatInterval, UserId, Effective, PlayableCheckedAtSeconds]()
			{
				if (bAllowEnter)
				{
					const double ElapsedSeconds = FMath::Max(0.0, FPlatformTime::Seconds() - PlayableCheckedAtSeconds);
					const int32 RemainingAtClick = FMath::Max(0,
						Effective.RemainTime - FMath::FloorToInt(ElapsedSeconds));
					UE_LOG(LogTapCompliance, Log,
						TEXT("[Flow] Step3: enter clicked, initialRemain=%d elapsed=%.2f actualRemain=%d"),
						Effective.RemainTime, ElapsedSeconds, RemainingAtClick);

					if (RemainingAtClick <= 0)
					{
						UE_LOG(LogTapCompliance, Log,
							TEXT("[Flow] Step3: enter blocked because actual remain time expired"));
						FPlayableResult Expired = Effective;
						Expired.RemainTime = 0;
						Expired.Title.Empty();
						Expired.Content.Empty();
						HandleStep3Result(UserId, Expired);
						return;
					}

					FComplianceState& State = FComplianceState::Get();
					State.CurrentPlayableResult = Effective;
					State.CurrentPlayableResult.RemainTime = RemainingAtClick;
					State.RemainSeconds.Set(RemainingAtClick);
					FCompliancePoll::Get().StartUp(HeartbeatInterval);
					Finish(TapComplianceConst::LOGIN_SUCCESS);
				}
				else
				{
					// 与 Unity BaseComplianceWorker.cs:86-88 对齐: 禁玩态点 "退出游戏" 由 SDK 主动退游戏,
					// 不依赖游戏侧自行监听 PERIOD_RESTRICT(1030). PERIOD_RESTRICT 已先在弹窗前派发供埋点 / 业务感知.
					UWorld* World = (GEngine && GEngine->GameViewport) ? GEngine->GameViewport->GetWorld() : nullptr;
					UKismetSystemLibrary::QuitGame(World, nullptr, EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
				}
			}),
		OnSwitchAccountCb,
		FOnHealthMinorTimeLimitClosed::CreateLambda([](){ /* 关闭等同上 */ })
	);

	if (!bAllowEnter)
	{
		Finish(TapComplianceConst::PERIOD_RESTRICT);
	}
}

// =============================================================================
// 离线兜底 (仅 policy_active==time_range)
// =============================================================================

FPlayableResult FComplianceAuthFlow::ComputeOfflinePlayable()
{
	const int64 ServerTime = FTapHttpTime::GetCurrentServerTime();
	const int64 EffectiveTime = ServerTime > 0
		? ServerTime
		: FComplianceState::Get().GetOfflineLocalUnixTime();

	UE_LOG(LogTapCompliance, Log,
		TEXT("[Flow] ComputeOfflinePlayable time source=%s unix=%lld"),
		ServerTime > 0 ? TEXT("server") : TEXT("local"), EffectiveTime);

	return ComputeOfflinePlayableAtInternal(FromUnixTimeLocal(EffectiveTime));
}

#if WITH_DEV_AUTOMATION_TESTS
FPlayableResult FComplianceAuthFlow::ComputeOfflinePlayableAt(const FDateTime& Now)
{
	return ComputeOfflinePlayableAtInternal(Now);
}
#endif

void FComplianceAuthFlow::ResolveHealthReminderText(
	const FPlayableResult& Playable,
	const FUserAntiConfig& UserAntiConfig,
	FText& OutTitle,
	FText& OutContent)
{
	FString Title   = Playable.Title;
	FString Content = Playable.Content;

	// ① 服务端文案为空 → 本地兜底 (按 RemainTime 二选一)
	//    对齐 Android TapComplianceSettings.generateAlertMessage:
	//      remainTime <= 0 ? Reject : Allow
	if (Title.IsEmpty() || Content.IsEmpty())
	{
		const FHealthReminderDesc& Desc = (Playable.RemainTime > 0)
			? UserAntiConfig.LocalConfig.TimeRangeConfig.UITipText.Allow
			: UserAntiConfig.LocalConfig.TimeRangeConfig.UITipText.Reject;
		if (Title.IsEmpty())   { Title   = Desc.TipTitle; }
		if (Content.IsEmpty()) { Content = Desc.TipDescription; }
	}

	// ② 占位符替换 (服务端 description_plain 已渲染好不会含占位符, Contains 自然 false;
	//    只有本地配置文案才会触发).
	//    切分: 只对占位符后半段做 "分钟→秒" 替换, 避免误伤前缀里出现的 "60 分钟" 字样
	//    (对齐 Android getRestTime substring1/substring2 切分语义).
	const int32 Idx = Content.Find(TEXT("# ${remaining} #"));
	if (Idx != INDEX_NONE)
	{
		FString Head = Content.Left(Idx);
		FString Tail = Content.Mid(Idx);

		if (Playable.RemainTime < 60)
		{
			Tail = Tail.Replace(TEXT("# ${remaining} #"), *FString::FromInt(Playable.RemainTime));
			Tail = Tail.Replace(TEXT("分钟"), TEXT("秒"));
		}
		else
		{
			const int32 Mins = FMath::CeilToInt(Playable.RemainTime / 60.0);
			Tail = Tail.Replace(TEXT("# ${remaining} #"), *FString::FromInt(Mins));
		}
		Content = Head + Tail;
	}

	OutTitle   = FText::FromString(Title);
	OutContent = FText::FromString(Content);
}

bool FComplianceAuthFlow::IsInLocalGameTime()
{
	return ComputeOfflinePlayable().RemainTime > 0;
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.
//
// Live HTTP 测试：把请求真正打到研发环境 (xdrnd)，验证签名 / 公共头 / 响应解析整条链路。
//
// 测试通过条件（与用户约定）：
//   - 网络层 OK（拿到 HTTP 响应，能解析出 envelope）即视为 PASS
//   - 服务端业务错误（如未实名、假身份证被拒）也算 PASS，只要错误体可解析
//   - 只有以下情况算 FAIL：
//       - 网络层失败（连接超时、DNS 解析失败、HTTP 5xx 等）
//       - 响应非法 JSON / 无 envelope
//       - 超时（默认 20s）
//
// 运行：
//   编辑器：Tools → Test Automation → TapCompliance.Network.Live.*
//   CLI:   UnrealEditor.exe <project> -ExecCmds="Automation RunTests TapCompliance.Network.Live; Quit" \
//                                     -unattended -nullrhi -log

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

// TestFixtures.h 含 token 凭证，已被 .gitignore；没复制 example 时跳过整个 Live 测试集合
#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX) && __has_include("TestFixtures.h")

#include "ComplianceConst.h"
#include "ComplianceLiveTestHelpers.h"
#include "ComplianceModels.h"
#include "ComplianceNetwork.h"
#include "ComplianceState.h"
#include "Internal/Http/TapHttpCommonParams.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"
#include "TestFixtures.h"

namespace
{
	/** 测试前置：切研发 Host + 初始化 TapBasic / TapConfig + 清理 ComplianceState + 注入 TapLogin token */
	void SetupResearchEnvAndInjectToken()
	{
		// 1. 切研发 Host，避免污染生产
		TapComplianceConst::SetHostOverride(TapComplianceConst::HOST_CN_RND);

		// 1.5. FComplianceNetwork 的 HttpClient 在 Initialize() 里一次性捕获 Domain,
		//      模块启动时已用默认 PROD 域建好；这里 Shutdown + Initialize 重建一次，
		//      让它读到最新的 HOST_CN_RND。
		FComplianceNetwork::Shutdown();
		FComplianceNetwork::Initialize();

		// 2. 初始化 TapBasic 公参（ClientID + ClientToken）；缺 ClientToken 时无法计算 X-Tap-Sign,
		//    服务端会回 invalid_client。
		FTapHttpCommonParams::Initialize(
			TapComplianceTestFixtures::Tap_ClientID,
			TapComplianceTestFixtures::Tap_ClientToken);

		// 3. 同步喂给 FTapTapSdkOptions，否则 TapLogin Initialize 会因为 ClientID 为空而 bInitialized=false,
		//    进而 BuildAuthorizationHeader 直接返回空串导致 FastAuth 拿不到 MAC 头。
		//    bIsRND=true 让 TapLogin 走研发环境 host (虽然测试里不实际用到 OAuth 流程)。
		{
			FTapTapSdkOptions Cfg;
			Cfg.ClientID    = TapComplianceTestFixtures::Tap_ClientID;
			Cfg.ClientToken = TapComplianceTestFixtures::Tap_ClientToken;
			Cfg.Region      = ETapRegion::CN;
			FTapSdkConfigState::SetRNDEnvironment(true);
			FTapSdkConfigState::SetCurrentConfig(Cfg);
		}

		// 4. ComplianceState 是单例，每个 case 跑前清一遍
		FComplianceState::Get().Verification = FComplianceVerification();

		// 5. 注入 TapLogin MAC token（用于 FastAuth / 任何 BuildAuthorizationHeader 调用）
		TArray<FString> Scopes;
		Scopes.Add(TapComplianceConst::SCOPE_COMPLIANCE);
		FTapLoginInternal::InjectTokenForTest(
			TapComplianceTestFixtures::TapLogin_Kid,
			TapComplianceTestFixtures::TapLogin_AccessToken,
			TapComplianceTestFixtures::TapLogin_MacKey,
			TapComplianceTestFixtures::TapLogin_MacAlgorithm,
			Scopes);
	}

	/** 业务通用断言：网络层 OK 或拿到结构化错误体都算 PASS；失败时把完整响应体也打出来 */
	void AssertNetworkLayerOk(
		FAutomationTestBase& Test,
		const TCHAR* Label,
		bool bBusinessSuccess,
		const FComplianceError& Err)
	{
		if (bBusinessSuccess)
		{
			Test.AddInfo(FString::Printf(TEXT("[%s] business success"), Label));
			return;
		}
		// 业务失败：把完整诊断信息打出来（HTTP 状态 + Error + Code + Desc + 原始 body）
		Test.AddInfo(FString::Printf(
			TEXT("[%s] business failed: HTTP=%d Error=%s Code=%d Msg=%s Desc=%s"),
			Label, Err.HttpStatus, *Err.Error, Err.ErrorCode, *Err.Msg, *Err.ErrorDescription));
		if (!Err.RawResponse.IsEmpty())
		{
			Test.AddInfo(FString::Printf(
				TEXT("[%s] raw response: %s"), Label, *Err.RawResponse));
		}

		// 必须能拿到结构化错误体（Error 字段非空）
		const bool bHasStructuredError = !Err.Error.IsEmpty();
		Test.TestTrue(
			FString::Printf(TEXT("[%s] 业务失败应有结构化错误体"), Label),
			bHasStructuredError);
	}
}

// =============================================================================
// 1. FetchGlobalConfig（无 token）
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveFetchGlobalConfigTest,
	"TapCompliance.Network.Live.FetchGlobalConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveFetchGlobalConfigTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	bool* bDone = new bool(false); // 在 latent command 与回调间共享
	FComplianceNetwork::FetchGlobalConfig(
		TapComplianceTestFixtures::TestUserId,
		[this, bDone](bool bOk, const FRealNameConfigResult& Cfg, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("FetchGlobalConfig"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[FetchGlobalConfig] useManual=%s tapAuth.title=%s manualAuth.title=%s authWaiting.title=%s authFailed.title=%s"),
					Cfg.bUseManual ? TEXT("true") : TEXT("false"),
					*Cfg.RealNameText.TapAuthTip.Title,
					*Cfg.RealNameText.ManualAuthTip.Title,
					*Cfg.RealNameText.AuthWaitingTip.Title,
					*Cfg.RealNameText.ManualAuthFailedTip.Title));
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("FetchGlobalConfig")));
	return true;
}

// =============================================================================
// 2. FetchVerification（无 token，未实名用户预期 status=failed）
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveFetchVerificationTest,
	"TapCompliance.Network.Live.FetchVerification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveFetchVerificationTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	bool* bDone = new bool(false);
	FComplianceNetwork::FetchVerification(
		TapComplianceTestFixtures::TestUserId,
		[this, bDone](bool bOk, const FComplianceVerification& V, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("FetchVerification"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[FetchVerification] status=%s ageLimit=%d isAdult=%s tokenLen=%d"),
					*V.Status, V.AgeLimit,
					V.bIsAdult ? TEXT("true") : TEXT("false"),
					V.ComplianceToken.Len()));
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("FetchVerification")));
	return true;
}

// =============================================================================
// 3. FetchVerificationManual（假身份证，预期业务错误）
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveFetchVerificationManualTest,
	"TapCompliance.Network.Live.FetchVerificationManual",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveFetchVerificationManualTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	bool* bDone = new bool(false);
	FComplianceNetwork::FetchVerificationManual(
		TapComplianceTestFixtures::TestUserId,
		TapComplianceTestFixtures::FakeName,
		TapComplianceTestFixtures::FakeIdCard,
		[this, bDone](bool bOk, const FComplianceVerification& V, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("FetchVerificationManual"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[FetchVerificationManual] status=%s ageLimit=%d isAdult=%s tokenLen=%d"),
					*V.Status, V.AgeLimit,
					V.bIsAdult ? TEXT("true") : TEXT("false"),
					V.ComplianceToken.Len()));
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("FetchVerificationManual")));
	return true;
}

// =============================================================================
// 4. FetchVerificationByTapToken（FastAuth，依赖 TapLogin token）
//    成功时把 v2 ComplianceToken 写入 ComplianceState，给后续 token-required case 用
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveFastAuthTest,
	"TapCompliance.Network.Live.FastAuth",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveFastAuthTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	bool* bDone = new bool(false);
	FComplianceNetwork::FetchVerificationByTapToken(
		TapComplianceTestFixtures::TestUserId,
		[this, bDone](bool bOk, const FComplianceVerification& V, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("FastAuth"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[FastAuth] status=%s ageLimit=%d isAdult=%s tokenLen=%d"),
					*V.Status, V.AgeLimit,
					V.bIsAdult ? TEXT("true") : TEXT("false"),
					V.ComplianceToken.Len()));

				if (!V.ComplianceToken.IsEmpty())
				{
					// 把 v2 token 暂存进 ComplianceState，供 token-required case 复用
					FComplianceState::Get().Verification = V;
					AddInfo(TEXT("[FastAuth] v2 ComplianceToken stored in FComplianceState"));
				}
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("FastAuth"), /*Timeout*/ 30.0));
	return true;
}

// =============================================================================
// 5. FetchUserAntiConfig（需 v2 token，跳过条件：ComplianceState 里没 token）
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveFetchUserAntiConfigTest,
	"TapCompliance.Network.Live.FetchUserAntiConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveFetchUserAntiConfigTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	// 即使 ComplianceState 里没 token，也照样发请求 —— 验证服务端返回 invalid_client 是
	// 一个结构化错误，依然算 "网络层 OK"。
	bool* bDone = new bool(false);
	FComplianceNetwork::FetchUserAntiConfig(
		TapComplianceTestFixtures::TestUserId,
		[this, bDone](bool bOk, const FUserAntiConfig& Cfg, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("FetchUserAntiConfig"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[FetchUserAntiConfig] real_name(ageLimit=%d isAdult=%s) anti_addiction(active=%s model=%s heartbeat=%ds) content_rating(allow=%s) time_range(start=%s end=%s holidays=%d)"),
					Cfg.UserState.AgeLimit,
					Cfg.UserState.bIsAdult ? TEXT("true") : TEXT("false"),
					*Cfg.Policy.Active, *Cfg.Policy.Model, Cfg.Policy.HeartbeatInterval,
					Cfg.AgeCheckResult.bAllow ? TEXT("true") : TEXT("false"),
					*Cfg.LocalConfig.TimeRangeConfig.TimeStart,
					*Cfg.LocalConfig.TimeRangeConfig.TimeEnd,
					Cfg.LocalConfig.TimeRangeConfig.Holidays.Num()));
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("FetchUserAntiConfig")));
	return true;
}

// =============================================================================
// 6. Heartbeat（需 v2 token）
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveHeartbeatTest,
	"TapCompliance.Network.Live.Heartbeat",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveHeartbeatTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Short).Replace(
		TEXT("-"), TEXT(""));

	bool* bDone = new bool(false);
	FComplianceNetwork::Heartbeat(
		TapComplianceTestFixtures::TestUserId,
		SessionId,
		[this, bDone](bool bOk, const FPlayableResult& R, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("Heartbeat"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[Heartbeat] remain=%ds title=%s content=%s"),
					R.RemainTime, *R.Title, *R.Content));
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("Heartbeat")));
	return true;
}

// =============================================================================
// 7. CheckPayable
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveCheckPayableTest,
	"TapCompliance.Network.Live.CheckPayable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveCheckPayableTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	bool* bDone = new bool(false);
	FComplianceNetwork::CheckPayable(
		TapComplianceTestFixtures::TestUserId,
		TapComplianceTestFixtures::PaymentAmountFen,
		[this, bDone](bool bOk, const FPayableResult& R, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("CheckPayable"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[CheckPayable] allow=%s title=%s content=%s"),
					R.bAllow ? TEXT("true") : TEXT("false"),
					*R.Title, *R.Content));
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("CheckPayable")));
	return true;
}

// =============================================================================
// 8. SubmitPayment
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceLiveSubmitPaymentTest,
	"TapCompliance.Network.Live.SubmitPayment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FComplianceLiveSubmitPaymentTest::RunTest(const FString& /*Parameters*/)
{
	SetupResearchEnvAndInjectToken();

	bool* bDone = new bool(false);
	FComplianceNetwork::SubmitPayment(
		TapComplianceTestFixtures::TestUserId,
		TapComplianceTestFixtures::PaymentAmountFen,
		[this, bDone](bool bOk, const FComplianceError& Err)
		{
			AssertNetworkLayerOk(*this, TEXT("SubmitPayment"), bOk, Err);
			if (bOk)
			{
				AddInfo(FString::Printf(
					TEXT("[SubmitPayment] amount=%lld fen submitted OK"),
					TapComplianceTestFixtures::PaymentAmountFen));
			}
			*bDone = true;
		});

	ADD_LATENT_AUTOMATION_COMMAND(FWaitForFlagLatentCommand(
		bDone, this, TEXT("SubmitPayment")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

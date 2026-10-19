// Copyright TapTap. All Rights Reserved.

#include "ComplianceNetwork.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "ComplianceState.h"
#include "TapComplianceModule.h"
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"
#include "Internal/Http/TapHttp.h"
#include "Internal/Http/TapHttpAuthorizationService.h"
#include "Internal/Http/TapHttpRetryStrategy.h"
#include "Internal/Http/TapHttpTime.h"
#include "Dom/JsonObject.h"

// =============================================================================
// HttpClient 单例
// =============================================================================

namespace
{
	class FComplianceTapTokenAuthorizationService : public ITapHttpAuthorizationService
	{
	public:
		explicit FComplianceTapTokenAuthorizationService(const FTapAccessToken& InToken)
			: Token(InToken)
		{
		}

		virtual FString ObtainAuthorization(const FString& Url, const FString& Method) override
		{
			FString Host;
			FString Port = TEXT("443");
			FString Path;

			FString Tmp = Url;
			if (Tmp.StartsWith(TEXT("https://")))
			{
				Tmp.RightChopInline(8, EAllowShrinking::No);
				Port = TEXT("443");
			}
			else if (Tmp.StartsWith(TEXT("http://")))
			{
				Tmp.RightChopInline(7, EAllowShrinking::No);
				Port = TEXT("80");
			}

			int32 SlashIdx = INDEX_NONE;
			Tmp.FindChar(TEXT('/'), SlashIdx);
			if (SlashIdx == INDEX_NONE)
			{
				Host = Tmp;
				Path = TEXT("/");
			}
			else
			{
				Host = Tmp.Left(SlashIdx);
				Path = Tmp.Mid(SlashIdx);
			}

			return FTapLoginInternal::BuildAuthorizationHeaderForToken(
				Token,
				Method,
				Path,
				Host,
				Port,
				FTapHttpTime::GetCurrentTime());
		}

	private:
		FTapAccessToken Token;
	};
}

TSharedPtr<FTapHttp> FComplianceNetwork::HttpClient = nullptr;
FString FComplianceNetwork::HttpClientHost;

void FComplianceNetwork::Initialize()
{
	check(IsInGameThread());

	const FString Host = TapComplianceConst::GetHostCN();
	if (HttpClient.IsValid() && HttpClientHost == Host)
	{
		return;
	}

	if (HttpClient.IsValid())
	{
		UE_LOG(LogTapCompliance, Log,
			TEXT("[Network] Rebuild HttpClient for host change: %s -> %s"),
			*HttpClientHost, *Host);
	}

	HttpClient = FTapHttp::NewBuilder(TEXT("TapCompliance"), TEXT("1.0"))
		->Domain(Host)
		->Build();
	HttpClientHost = Host;
}

void FComplianceNetwork::Shutdown()
{
	HttpClient.Reset();
	HttpClientHost.Empty();
}

TSharedPtr<FTapHttp> FComplianceNetwork::GetHttpClient()
{
	Initialize();
	if (!HttpClient.IsValid())
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[Network] HttpClient is not available"));
	}
	return HttpClient;
}

FComplianceError FComplianceNetwork::MakeNetworkUnavailableError(const FString& Message)
{
	FComplianceError Err;
	Err.TransportErrorType = ETapHttpErrorType::NetworkError;
	Err.ErrorDescription = Message;
	return Err;
}

#if WITH_DEV_AUTOMATION_TESTS
FString FComplianceNetwork::GetCurrentHostForTest()
{
	return HttpClientHost;
}
#endif

// =============================================================================
// 端点 1: FetchGlobalConfig (GET 无 token)
// =============================================================================

void FComplianceNetwork::FetchGlobalConfig(const FString& UserId, FOnRealNameConfig OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] GET %s"), *TapComplianceConst::PATH_GLOBAL_CONFIG);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FRealNameConfigResult(),
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	Client->Get(
		TapComplianceConst::PATH_GLOBAL_CONFIG,
		{{TEXT("user_identifier"), UserId}},
		/*ExtraHeaders*/ {},
		/*bEnableAuthorization*/ false,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateFixed(3)),
		FTapHttpJsonResultDelegate::CreateLambda(
			[OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FRealNameConfigResult Result;
				FComplianceError Err;
				if (R.IsSuccess()) Result.FromJson(R.GetData());
				else Err = FComplianceError::FromException(R.GetException());
				if (OnDone) OnDone(R.IsSuccess(), Result, Err);
			}));
}

// =============================================================================
// 端点 2: FetchVerification (GET 无 token)
// =============================================================================

void FComplianceNetwork::FetchVerification(const FString& UserId, FOnVerification OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] GET %s"), *TapComplianceConst::PATH_VERIFICATION);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FComplianceVerification(),
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	Client->Get(
		TapComplianceConst::PATH_VERIFICATION,
		{{TEXT("user_identifier"), UserId}},
		/*ExtraHeaders*/ {},
		/*bEnableAuthorization*/ false,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateFixed(3)),
		FTapHttpJsonResultDelegate::CreateLambda(
			[UserId, OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FComplianceVerification Result;
				FComplianceError Err;
				if (R.IsSuccess())
				{
					Result.FromJson(R.GetData());
					Result.UserId = UserId;
				}
				else
				{
					Err = FComplianceError::FromException(R.GetException());
				}
				if (OnDone) OnDone(R.IsSuccess(), Result, Err);
			}));
}

// =============================================================================
// 端点 2.5: FetchVerificationByTapToken (FastAuth — bEnableAuthorization=true)
// AuthorizationService 全局注册后自动 MAC + invalid_time 自动重签
// =============================================================================

void FComplianceNetwork::FetchVerificationByTapToken(const FString& UserId, FOnVerification OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] GET (FastAuth) %s"), *TapComplianceConst::PATH_VERIFICATION_TAPTAP);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FComplianceVerification(),
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	Client->Get(
		TapComplianceConst::PATH_VERIFICATION_TAPTAP,
		{{TEXT("user_identifier"), UserId}},
		/*ExtraHeaders*/ {},
		/*bEnableAuthorization*/ true,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateFixed(1)),
		FTapHttpJsonResultDelegate::CreateLambda(
			[UserId, OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FComplianceVerification V;
				FComplianceError E;
				if (R.IsSuccess())
				{
					V.FromJson(R.GetData());
					V.UserId = UserId;
				}
				else
				{
					E = FComplianceError::FromException(R.GetException());
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Network] FastAuth failed: error=%s desc=%s"),
						*E.Error, *E.ErrorDescription);
				}
				if (OnDone) OnDone(R.IsSuccess(), V, E);
			}));
}

// =============================================================================
// 端点 3: FetchVerificationManual (POST 无 token)
// =============================================================================

void FComplianceNetwork::FetchVerificationByTapToken(
	const FString& UserId,
	const FTapAccessToken& Token,
	FOnVerification OnDone)
{
	UE_LOG(LogTapCompliance, Log,
		TEXT("[Network] GET (FastAuth token-only) %s"), *TapComplianceConst::PATH_VERIFICATION_TAPTAP);

	if (!Token.IsValidForMac())
	{
		if (OnDone) OnDone(false, FComplianceVerification(),
			MakeNetworkUnavailableError(TEXT("TapLogin access token is missing MAC fields")));
		return;
	}

	const FString Host = TapComplianceConst::GetHostCN();
	TSharedRef<ITapHttpAuthorizationService> AuthService =
		StaticCastSharedRef<ITapHttpAuthorizationService>(
			MakeShared<FComplianceTapTokenAuthorizationService>(Token));
	TSharedPtr<FTapHttp> Client = FTapHttp::NewBuilder(TEXT("TapCompliance"), TEXT("1.0"))
		->Domain(Host)
		->Authorization(AuthService)
		->Build();

	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FComplianceVerification(),
			MakeNetworkUnavailableError(TEXT("TapCompliance token-only network client not initialized")));
		return;
	}

	Client->Get(
		TapComplianceConst::PATH_VERIFICATION_TAPTAP,
		{{TEXT("user_identifier"), UserId}},
		/*ExtraHeaders*/ {},
		/*bEnableAuthorization*/ true,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateFixed(1)),
		FTapHttpJsonResultDelegate::CreateLambda(
			[UserId, Client, OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				// Keep the per-request token-only FTapHttp alive until the async callback runs.
				(void)Client;
				FComplianceVerification V;
				FComplianceError E;
				if (R.IsSuccess())
				{
					V.FromJson(R.GetData());
					V.UserId = UserId;
				}
				else
				{
					E = FComplianceError::FromException(R.GetException());
					UE_LOG(LogTapCompliance, Warning,
						TEXT("[Network] FastAuth token-only failed: error=%s desc=%s"),
						*E.Error, *E.ErrorDescription);
				}
				if (OnDone) OnDone(R.IsSuccess(), V, E);
			}));
}

void FComplianceNetwork::FetchVerificationManual(
	const FString& UserId,
	const FString& Name,
	const FString& IdCard,
	FOnVerification OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] POST %s"), *TapComplianceConst::PATH_VERIFICATION_MANUAL);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FComplianceVerification(),
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	auto Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"),            Name);
	Body->SetStringField(TEXT("idcard"),          IdCard);
	Body->SetStringField(TEXT("user_identifier"), UserId);

	Client->PostJson(
		TapComplianceConst::PATH_VERIFICATION_MANUAL,
		/*QueryParams*/ {},
		/*ExtraHeaders*/ {},
		Body,
		/*bEnableAuthorization*/ false,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateNone()),
		FTapHttpJsonResultDelegate::CreateLambda(
			[UserId, OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FComplianceVerification Result;
				FComplianceError Err;
				if (R.IsSuccess())
				{
					Result.FromJson(R.GetData());
					Result.UserId = UserId;
				}
				else
				{
					Err = FComplianceError::FromException(R.GetException());
				}
				if (OnDone) OnDone(R.IsSuccess(), Result, Err);
			}));
}

// =============================================================================
// 端点 4: FetchUserAntiConfig (GET 带 Anti-Addiction-Token)
// =============================================================================

void FComplianceNetwork::FetchUserAntiConfig(const FString& UserId, FOnUserAntiConfig OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] GET %s"), *TapComplianceConst::PATH_USER_ANTI_CONFIG);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FUserAntiConfig(),
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	TMap<FString, FString> Extra;
	const FString& Token = FComplianceState::Get().Verification.ComplianceToken;
	if (!Token.IsEmpty())
	{
		Extra.Add(TapComplianceConst::HEADER_ANTI_ADDICTION_TOKEN, Token);
	}
	else
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Network] FetchUserAntiConfig: ComplianceToken is empty"));
	}

	Client->Get(
		TapComplianceConst::PATH_USER_ANTI_CONFIG,
		{{TEXT("user_identifier"), UserId}, {TEXT("platform"), TEXT("pc")}},
		Extra,
		/*bEnableAuthorization*/ false,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateFixed(3)),
		FTapHttpJsonResultDelegate::CreateLambda(
			[OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FUserAntiConfig Result;
				FComplianceError Err;
				if (R.IsSuccess()) Result.FromJson(R.GetData());
				else Err = FComplianceError::FromException(R.GetException());
				if (OnDone) OnDone(R.IsSuccess(), Result, Err);
			}));
}

// =============================================================================
// 端点 5: Heartbeat (POST 带 Anti-Addiction-Token)
// =============================================================================

void FComplianceNetwork::Heartbeat(
	const FString& UserId,
	const FString& SessionId,
	FOnPlayable OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] POST %s"), *TapComplianceConst::PATH_HEARTBEAT);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FPlayableResult(),
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	auto Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("session_id"),      SessionId);
	Body->SetStringField(TEXT("user_identifier"), UserId);

	TMap<FString, FString> Extra;
	const FString& Token = FComplianceState::Get().Verification.ComplianceToken;
	if (!Token.IsEmpty())
	{
		Extra.Add(TapComplianceConst::HEADER_ANTI_ADDICTION_TOKEN, Token);
	}
	else
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Network] Heartbeat: ComplianceToken is empty"));
	}

	Client->PostJson(
		TapComplianceConst::PATH_HEARTBEAT,
		/*QueryParams*/ {},
		Extra,
		Body,
		/*bEnableAuthorization*/ false,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateNone()),
		FTapHttpJsonResultDelegate::CreateLambda(
			[OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FPlayableResult Result;
				FComplianceError Err;
				if (R.IsSuccess()) Result.FromJson(R.GetData());
				else Err = FComplianceError::FromException(R.GetException());
				if (OnDone) OnDone(R.IsSuccess(), Result, Err);
			}));
}

// =============================================================================
// 端点 6: CheckPayable (GET 带 Anti-Addiction-Token)
// =============================================================================

void FComplianceNetwork::CheckPayable(
	const FString& UserId,
	int64 AmountFen,
	FOnPayable OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] GET %s"), *TapComplianceConst::PATH_PAYABLE);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false, FPayableResult(),
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	TMap<FString, FString> Extra;
	const FString& Token = FComplianceState::Get().Verification.ComplianceToken;
	if (!Token.IsEmpty())
	{
		Extra.Add(TapComplianceConst::HEADER_ANTI_ADDICTION_TOKEN, Token);
	}
	else
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Network] CheckPayable: ComplianceToken is empty"));
	}

	Client->Get(
		TapComplianceConst::PATH_PAYABLE,
		{{TEXT("user_identifier"), UserId}, {TEXT("amount"), LexToString(AmountFen)}},
		Extra,
		/*bEnableAuthorization*/ false,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateFixed(3)),
		FTapHttpJsonResultDelegate::CreateLambda(
			[OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FPayableResult Result;
				FComplianceError Err;
				bool bSuccess = R.IsSuccess();
				if (bSuccess && !Result.FromJson(R.GetData()))
				{
					bSuccess = false;
					Err.TransportErrorType = ETapHttpErrorType::InvalidResponse;
					Err.ErrorDescription = TEXT("Invalid payment check response");
				}
				else if (!bSuccess) Err = FComplianceError::FromException(R.GetException());
				if (OnDone) OnDone(bSuccess, Result, Err);
			}));
}

// =============================================================================
// 端点 7: SubmitPayment (POST 带 Anti-Addiction-Token)
// =============================================================================

void FComplianceNetwork::SubmitPayment(
	const FString& UserId,
	int64 AmountFen,
	FOnVoid OnDone)
{
	UE_LOG(LogTapCompliance, Log, TEXT("[Network] POST %s"), *TapComplianceConst::PATH_SUBMIT_PAYMENT);

	TSharedPtr<FTapHttp> Client = GetHttpClient();
	if (!Client.IsValid())
	{
		if (OnDone) OnDone(false,
			MakeNetworkUnavailableError(TEXT("TapCompliance network client not initialized")));
		return;
	}

	auto Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("amount"),          static_cast<double>(AmountFen));
	Body->SetStringField(TEXT("user_identifier"), UserId);

	TMap<FString, FString> Extra;
	const FString& Token = FComplianceState::Get().Verification.ComplianceToken;
	if (!Token.IsEmpty())
	{
		Extra.Add(TapComplianceConst::HEADER_ANTI_ADDICTION_TOKEN, Token);
	}
	else
	{
		UE_LOG(LogTapCompliance, Warning,
			TEXT("[Network] SubmitPayment: ComplianceToken is empty"));
	}

	Client->PostJson(
		TapComplianceConst::PATH_SUBMIT_PAYMENT,
		/*QueryParams*/ {},
		Extra,
		Body,
		/*bEnableAuthorization*/ false,
		FTapHttpRetryStrategy::CreateDefault(FTapHttpBackoffStrategy::CreateNone()),
		FTapHttpJsonResultDelegate::CreateLambda(
			[OnDone = MoveTemp(OnDone)](const FTapHttpJsonResult& R)
			{
				FComplianceError Err;
				if (!R.IsSuccess()) Err = FComplianceError::FromException(R.GetException());
				if (OnDone) OnDone(R.IsSuccess(), Err);
			}));
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.
//
// 纯函数测试：验证 Host override。
// BuildUrl / BuildQueryParams / ParseEnvelope / SerializeJson 已迁移到 FTapHttp 接口层。

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "ComplianceConst.h"
#include "ComplianceModels.h"
#include "ComplianceNetwork.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"

// =============================================================================
// Host override 切换
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceHostOverrideTest,
	"TapCompliance.Network.Pure.HostOverride",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceHostOverrideTest::RunTest(const FString& /*Parameters*/)
{
	// 备份 FTapTapSdkOptions 和 override, 避免污染其他测试 / 真实业务.
	// GetHostCN() 会读取内部 RND 环境状态，需固定状态才能断言。
	const FTapTapSdkOptions OldConfig = FTapSdkConfigState::GetCurrentConfig();
	const bool bOldIsRND = FTapSdkConfigState::IsRNDEnvironment();
	const FString OldHostOverride = TapComplianceConst::GetHostOverride();

	// 强制 bIsRND=false, 让 "空串恢复默认" 这步能确定指向 PROD.
	FTapSdkConfigState::SetRNDEnvironment(false);
	TapComplianceConst::SetHostOverride(FString());

	TapComplianceConst::SetHostOverride(TapComplianceConst::HOST_CN_RND);
	TestEqual(TEXT("切到研发"),
		TapComplianceConst::GetHostCN(), TapComplianceConst::HOST_CN_RND);

	TapComplianceConst::SetHostOverride(FString());
	TestEqual(TEXT("空串 + bIsRND=false 回到 PROD"),
		TapComplianceConst::GetHostCN(), TapComplianceConst::HOST_CN_PROD);

	// 验证 bIsRND=true 时空串走 RND
	FTapSdkConfigState::SetRNDEnvironment(true);
	TestEqual(TEXT("空串 + bIsRND=true 自动切 RND"),
		TapComplianceConst::GetHostCN(), TapComplianceConst::HOST_CN_RND);

	// 还原
	FTapSdkConfigState::SetCurrentConfig(OldConfig);
	FTapSdkConfigState::SetRNDEnvironment(bOldIsRND);
	TapComplianceConst::SetHostOverride(OldHostOverride);
	return true;
}

// =============================================================================
// HttpClient 跟随 TapConfig Host 切换
// =============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceNetworkRebuildsForHostChangeTest,
	"TapCompliance.Network.Pure.RebuildsForHostChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceNetworkRebuildsForHostChangeTest::RunTest(const FString& /*Parameters*/)
{
	const FTapTapSdkOptions OldConfig = FTapSdkConfigState::GetCurrentConfig();
	const bool bOldIsRND = FTapSdkConfigState::IsRNDEnvironment();
	const FString OldHostOverride = TapComplianceConst::GetHostOverride();

	TapComplianceConst::SetHostOverride(FString());

	FTapSdkConfigState::SetRNDEnvironment(false);
	FComplianceNetwork::Shutdown();
	FComplianceNetwork::Initialize();
	TestEqual(TEXT("initial prod host"), FComplianceNetwork::GetCurrentHostForTest(),
		TapComplianceConst::HOST_CN_PROD);

	FTapSdkConfigState::SetRNDEnvironment(true);
	FComplianceNetwork::Initialize();
	TestEqual(TEXT("rebuilds when TapConfig switches to RND"), FComplianceNetwork::GetCurrentHostForTest(),
		TapComplianceConst::HOST_CN_RND);

	FTapSdkConfigState::SetCurrentConfig(OldConfig);
	FTapSdkConfigState::SetRNDEnvironment(bOldIsRND);
	TapComplianceConst::SetHostOverride(OldHostOverride);
	FComplianceNetwork::Shutdown();
	FComplianceNetwork::Initialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FComplianceErrorCacheFallbackPolicyTest,
	"TapCompliance.Network.Pure.CacheFallbackPolicy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FComplianceErrorCacheFallbackPolicyTest::RunTest(const FString& /*Parameters*/)
{
	FComplianceError Client4xx;
	Client4xx.HttpStatus = 403;
	Client4xx.Error = TEXT("business_code_error");
	Client4xx.ErrorCode = 403001;
	TestFalse(TEXT("4xx get-config-by-token errors must not use cached user anti-config"),
		Client4xx.IsRecoverableForCacheFallback());

	FComplianceError TokenExpired;
	TokenExpired.Error = TEXT("business_code_error");
	TokenExpired.ErrorCode = 200000;
	TestFalse(TEXT("token expired must not use cached user anti-config"),
		TokenExpired.IsRecoverableForCacheFallback());

	FComplianceError Network;
	Network.TransportErrorType = ETapHttpErrorType::NetworkError;
	TestTrue(TEXT("network errors may use cached user anti-config"),
		Network.IsRecoverableForCacheFallback());

	FComplianceError Timeout;
	Timeout.TransportErrorType = ETapHttpErrorType::Timeout;
	TestTrue(TEXT("timeout errors may use cached user anti-config"),
		Timeout.IsRecoverableForCacheFallback());

	FComplianceError Server5xx;
	Server5xx.TransportErrorType = ETapHttpErrorType::ServerError;
	Server5xx.HttpStatus = 500;
	Server5xx.ErrorCode = 500;
	TestTrue(TEXT("5xx errors may use cached user anti-config"),
		Server5xx.IsRecoverableForCacheFallback());
	TestTrue(TEXT("5xx errors may use cached global config"),
		Server5xx.IsRecoverableForGlobalConfigCacheFallback());

	FComplianceError Proxy5xx;
	Proxy5xx.TransportErrorType = ETapHttpErrorType::ServerError;
	Proxy5xx.HttpStatus = 502;
	TestTrue(TEXT("global config 5xx fallback must not depend on error name"),
		Proxy5xx.IsRecoverableForGlobalConfigCacheFallback());

	FComplianceError Invalid4xx;
	Invalid4xx.TransportErrorType = ETapHttpErrorType::InvalidResponse;
	Invalid4xx.HttpStatus = 404;
	TestFalse(TEXT("global config 4xx invalid response must not use cache"),
		Invalid4xx.IsRecoverableForGlobalConfigCacheFallback());

	FComplianceError InvalidResponse;
	InvalidResponse.TransportErrorType = ETapHttpErrorType::InvalidResponse;
	TestTrue(TEXT("global config parse failures without 4xx may use cached realname_config"),
		InvalidResponse.IsRecoverableForGlobalConfigCacheFallback());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCompliancePayableResponseTest,
	"TapCompliance.Network.Pure.PayableResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCompliancePayableResponseTest::RunTest(const FString&)
{
	FPayableResult Result;
	TestFalse(TEXT("Null response is a parsing error"), Result.FromJson(nullptr));
	auto Data = MakeShared<FJsonObject>();
	TestFalse(TEXT("Empty response is a parsing error"), Result.FromJson(Data));
	Data->SetStringField(TEXT("allow"), TEXT("false"));
	TestFalse(TEXT("Invalid allow type is a parsing error"), Result.FromJson(Data));
	Data->SetBoolField(TEXT("allow"), false);
	TestTrue(TEXT("Business denial is a valid result"), Result.FromJson(Data));
	TestFalse(TEXT("Denial stays false"), Result.bAllow);
	Data->SetBoolField(TEXT("allow"), true);
	TestTrue(TEXT("Approval is a valid result"), Result.FromJson(Data));
	TestTrue(TEXT("Approval stays true"), Result.bAllow);

	Data->RemoveField(TEXT("allow"));
	Data->SetStringField(TEXT("title"), TEXT("Payment limit"));
	Data->SetStringField(TEXT("description"), TEXT("<p>Single payment limit exceeded</p>"));
	Data->SetStringField(TEXT("description_plain"), TEXT("Single payment limit exceeded"));
	TestTrue(TEXT("Single payment restriction without allow is a valid result"), Result.FromJson(Data));
	TestFalse(TEXT("Missing allow resets a previous approval to denial"), Result.bAllow);
	TestEqual(TEXT("Single payment restriction preserves title"), Result.Title, FString(TEXT("Payment limit")));
	TestEqual(TEXT("Single payment restriction preserves plain content"), Result.Content,
		FString(TEXT("Single payment limit exceeded")));

	Data->SetStringField(TEXT("description"), TEXT("<p>Monthly payment limit exceeded</p>"));
	Data->SetStringField(TEXT("description_plain"), TEXT("Monthly payment limit exceeded"));
	TestTrue(TEXT("Monthly payment restriction without allow is a valid result"), Result.FromJson(Data));
	TestFalse(TEXT("Monthly payment restriction stays denied"), Result.bAllow);
	TestEqual(TEXT("Monthly payment restriction preserves plain content"), Result.Content,
		FString(TEXT("Monthly payment limit exceeded")));

	Data->SetStringField(TEXT("allow"), TEXT("false"));
	TestFalse(TEXT("Denial prompt does not excuse an invalid allow type"), Result.FromJson(Data));
	TestTrue(TEXT("Invalid response clears previous title"), Result.Title.IsEmpty());
	TestTrue(TEXT("Invalid response clears previous content"), Result.Content.IsEmpty());
	Data->SetNumberField(TEXT("allow"), 0);
	TestFalse(TEXT("Numeric allow is a parsing error"), Result.FromJson(Data));
	Data->SetField(TEXT("allow"), MakeShared<FJsonValueNull>());
	TestFalse(TEXT("Null allow is a parsing error"), Result.FromJson(Data));
	Data->RemoveField(TEXT("allow"));

	Data->RemoveField(TEXT("description_plain"));
	TestFalse(TEXT("Rich description alone is not a valid denial prompt"), Result.FromJson(Data));
	Data->SetNumberField(TEXT("description_plain"), 123);
	TestFalse(TEXT("Invalid denial content type is a parsing error"), Result.FromJson(Data));
	Data->SetStringField(TEXT("description_plain"), TEXT(""));
	TestFalse(TEXT("Empty denial content is a parsing error"), Result.FromJson(Data));
	Data->SetStringField(TEXT("description_plain"), TEXT("Monthly payment limit exceeded"));
	Data->RemoveField(TEXT("title"));
	TestFalse(TEXT("Missing denial title is a parsing error"), Result.FromJson(Data));
	Data->SetBoolField(TEXT("title"), false);
	TestFalse(TEXT("Invalid denial title type is a parsing error"), Result.FromJson(Data));
	Data->SetStringField(TEXT("title"), TEXT(""));
	TestFalse(TEXT("Empty denial title is a parsing error"), Result.FromJson(Data));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

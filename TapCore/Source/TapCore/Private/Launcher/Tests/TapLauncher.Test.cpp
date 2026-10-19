// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Launcher/TapLauncher.h"
#include "Internal/Launcher/TapLauncherDesktopImpl.h"
#include "Launcher/TapLauncherErrorDialogWidget.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"

// ── Helpers ───────────────────────────────────────────────────────────────

static void SetValidConfig()
{
	FTapTapSdkOptions Cfg;
	Cfg.ClientID        = TEXT("test_client_id");
	Cfg.ClientPublicKey = TEXT("test_pub_key");
	FTapSdkConfigState::SetCurrentConfig(Cfg);
}

static void SetEmptyConfig()
{
	FTapSdkConfigState::SetCurrentConfig(FTapTapSdkOptions{});
}

// ── Test 1: Empty ClientId → OnError ──────────────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLauncherEmptyClientIdTest,
	"TapSDK.Launcher.UTapLauncher.EmptyClientId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLauncherEmptyClientIdTest::RunTest(const FString&)
{
	FTapLauncherDesktopImpl::Reset();
	SetEmptyConfig();

	ETapLauncherInitResult GotCode = ETapLauncherInitResult::OK;
	FString GotMsg;
	// Error fires synchronously: validation branch executes OnError before
	// any AsyncTask is dispatched, so GotCode/GotMsg are set on return.
	UTapLauncher::IsLaunchedFromTapTapPC(
		FOnLauncherCheckSuccess(),
		FOnLauncherCheckError::CreateLambda([&](ETapLauncherInitResult C, const FString& M)
		{
			GotCode = C; GotMsg = M;
		}));

	TestEqual(TEXT("Empty ClientId → Unknown error code"),
		GotCode, ETapLauncherInitResult::Unknown);
	TestTrue(TEXT("Error message mentions ClientId or PublicKey"),
		GotMsg.Contains(TEXT("ClientId")) || GotMsg.Contains(TEXT("PublicKey")));
	return true;
}

// ── Test 2: IsPassedLauncherCheck default false ────────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLauncherDefaultNotPassedTest,
	"TapSDK.Launcher.UTapLauncher.DefaultNotPassed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLauncherDefaultNotPassedTest::RunTest(const FString&)
{
	FTapLauncherDesktopImpl::Reset();
	TestFalse(TEXT("IsPassedLauncherCheck should be false before any check"),
		UTapLauncher::IsPassedLauncherCheck());
	return true;
}

// ── Test 3: Duplicate call while in-progress is silent ────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLauncherDuplicateCallTest,
	"TapSDK.Launcher.UTapLauncher.DuplicateCallSilent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLauncherDuplicateCallTest::RunTest(const FString&)
{
	FTapLauncherDesktopImpl::Reset();
	SetValidConfig();

	int32 ErrorCount = 0;
	// First call: may fail synchronously (DLL not loaded) or go async
	UTapLauncher::IsLaunchedFromTapTapPC(
		FOnLauncherCheckSuccess(),
		FOnLauncherCheckError::CreateLambda([&](ETapLauncherInitResult, const FString&)
		{
			++ErrorCount;
		}));

	// Second call while in-progress → must be silent (no callback)
	int32 SecondCallbacks = 0;
	UTapLauncher::IsLaunchedFromTapTapPC(
		FOnLauncherCheckSuccess(),
		FOnLauncherCheckError::CreateLambda([&](ETapLauncherInitResult, const FString&)
		{
			++SecondCallbacks;
		}));

	// The second call must never fire a callback (silent when bInProgress==true)
	TestEqual(TEXT("Second in-progress call fires no callback"), SecondCallbacks, 0);
	return true;
}

// ── Test 4: Unity-compatible error message mapping ─────────────────────────
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLauncherErrorMessageMappingTest,
	"TapSDK.Launcher.UTapLauncher.ErrorMessageMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLauncherErrorMessageMappingTest::RunTest(const FString&)
{
	TestEqual(TEXT("NoPlatform message"),
		UTapLauncherErrorDialogWidget::GetErrorMessage(
			ETapLauncherInitResult::NoPlatform).ToString(),
		FString(TEXT("获取游戏信息失败，请下载 TapTap 客户端后重新启动游戏")));
	TestEqual(TEXT("NotLaunchedByPlatform message"),
		UTapLauncherErrorDialogWidget::GetErrorMessage(
			ETapLauncherInitResult::NotLaunchedByPlatform).ToString(),
		FString(TEXT("获取游戏信息失败，请从 TapTap 客户端重新启动游戏")));
	TestEqual(TEXT("PlatformVersionMismatch message"),
		UTapLauncherErrorDialogWidget::GetErrorMessage(
			ETapLauncherInitResult::PlatformVersionMismatch).ToString(),
		FString(TEXT("客户端版本不匹配，请将 TapTap 客户端升级到最新版本")));
	TestEqual(TEXT("Unknown message"),
		UTapLauncherErrorDialogWidget::GetErrorMessage(
			ETapLauncherInitResult::Unknown).ToString(),
		FString(TEXT("本地发生未知错误，请从 TapTap 客户端重新启动游戏")));
	TestEqual(TEXT("FailedGeneric fallback message"),
		UTapLauncherErrorDialogWidget::GetErrorMessage(
			ETapLauncherInitResult::FailedGeneric).ToString(),
		FString(TEXT("发生未知错误，请从 TapTap 客户端重新启动游戏")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

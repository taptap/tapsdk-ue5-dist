// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Tests/EnsureScope.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Platform/ITapLicensePlatform.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLicenseFactoryCreatesPlatformTest,
	"TapSDK.License.Factory.CreatesPlatform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLicenseFactoryCreatesPlatformTest::RunTest(const FString&)
{
	FTapLicensePlatformFactory::ResetPlatform();
	TSharedPtr<ITapLicensePlatform> P = FTapLicensePlatformFactory::CreatePlatform();
	TestTrue(TEXT("CreatePlatform 返回非空"), P.IsValid());

#if PLATFORM_WINDOWS
	TestEqual(TEXT("Win64 平台名"), P->GetPlatformName(), FString(TEXT("Win64")));
#elif PLATFORM_MAC || PLATFORM_LINUX
	TestEqual(TEXT("桌面 stub 平台名"), P->GetPlatformName(), FString(TEXT("Desktop-Stub")));
#elif PLATFORM_ANDROID
	TestEqual(TEXT("Android 平台名"), P->GetPlatformName(), FString(TEXT("Android")));
#elif PLATFORM_IOS
	TestEqual(TEXT("iOS 平台名"), P->GetPlatformName(), FString(TEXT("iOS")));
#endif

	// 单例：第二次返回同一实例
	TSharedPtr<ITapLicensePlatform> P2 = FTapLicensePlatformFactory::CreatePlatform();
	TestEqual(TEXT("工厂单例"), P.Get(), P2.Get());

	FTapLicensePlatformFactory::ResetPlatform();
	return true;
}

#if PLATFORM_WINDOWS

#include "TapLicense.h"
#include "Internal/TapLicenseInternal.h"
#include "TapLicenseCallbackManager.h"
#include "Internal/Launcher/TapLauncherDesktopImpl.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapLicenseFacadeDelegatesToWin64Test,
	"TapSDK.License.Factory.FacadeDelegatesToWin64",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapLicenseFacadeDelegatesToWin64Test::RunTest(const FString&)
{
	FTapLicensePlatformFactory::ResetPlatform();
	FTapLauncherDesktopImpl::Reset();      // IsPassedLauncherCheck() == false
	FTapLicenseInternal::SetupCallbacks();

	bool bFailed = false;
	FDelegateHandle H = FTapLicenseCallbackManager::Get().TestFailed.AddLambda([&]{ bFailed = true; });
	FEnsureScope EnsureScope;
	UTapLicense::CheckLicense(false);      // 未调用启动校验，报告前置条件错误

	TestEqual(TEXT("Facade CheckLicense triggers one ensure"), EnsureScope.GetCount(), 1);
	FTapLicenseCallbackManager::Get().TestFailed.Remove(H);

	TestFalse(TEXT("门面委托后 CheckLicense → 报告前置条件错误且不广播业务回调"), bFailed);
	FTapLicensePlatformFactory::ResetPlatform();
	return true;
}

#endif // PLATFORM_WINDOWS

#endif // WITH_DEV_AUTOMATION_TESTS

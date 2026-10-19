// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX) && WITH_DEV_AUTOMATION_TESTS

#include "Desktop/TapCloudSaveDesktopWrapper.h"

/**
 * Exercises the error / not-ready paths in TapCloudSaveDesktopWrapper.
 *
 * Strategy: force Shutdown() so all cached function pointers are nulled out,
 * then call each public API and verify it returns the documented "safe"
 * sentinel instead of dereferencing a null pointer.
 *
 * NOTE: this test mutates global wrapper state — it Initialize()s the wrapper
 * back at the end so subsequent tests (and the editor session) keep working.
 * It is platform-guarded to Desktop targets only because the wrapper does not
 * link on iOS / Android.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapCloudSaveWrapperNotReadyTest,
	"TapSDK.CloudSave.Desktop.WrapperNotReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapCloudSaveWrapperNotReadyTest::RunTest(const FString& /*Parameters*/)
{
	// Force the wrapper into the "not loaded" state regardless of previous tests.
	TapCloudSaveDesktopWrapper::Shutdown();

	TestFalse(TEXT("IsReady() == false after Shutdown"),
		TapCloudSaveDesktopWrapper::IsReady());

	TestEqual(TEXT("CallInitialize returns -1 when wrapper not ready"),
		TapCloudSaveDesktopWrapper::CallInitialize(TEXT("{}")), -1);
	TestEqual(TEXT("UpdateAccessToken returns -1 when wrapper not ready"),
		TapCloudSaveDesktopWrapper::UpdateAccessToken(TEXT("{}")), -1);

	const FString ListResp = TapCloudSaveDesktopWrapper::CallGetArchiveList();
	TestTrue(TEXT("CallGetArchiveList returns empty FString when wrapper not ready"),
		ListResp.IsEmpty());

	bool bSucceeded = true;
	TArray<uint8> Bytes;
	FString ErrJson;
	TapCloudSaveDesktopWrapper::CallGetArchiveData(TEXT("u"), TEXT("f"), bSucceeded, Bytes, ErrJson);
	TestFalse(TEXT("CallGetArchiveData bSucceeded == false when wrapper not ready"), bSucceeded);
	TestTrue(TEXT("CallGetArchiveData error JSON non-empty when wrapper not ready"),
		!ErrJson.IsEmpty());
	TestTrue(TEXT("CallGetArchiveData error JSON contains 'wrapper'"),
		ErrJson.Contains(TEXT("wrapper")));

	// Restore the wrapper so later tests and the live editor session can
	// continue using the native SDK without manual re-init.
	const bool bInitialized = TapCloudSaveDesktopWrapper::Initialize();
#if PLATFORM_MAC
	TestTrue(TEXT("Mac CloudSave dylib loads and exports resolve"), bInitialized);
	TestTrue(TEXT("Mac CloudSave wrapper is ready after restore"),
		TapCloudSaveDesktopWrapper::IsReady());
#endif

	return true;
}

#endif // (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX) && WITH_DEV_AUTOMATION_TESTS

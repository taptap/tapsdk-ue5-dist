// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)
#include "Desktop/TapQRCodeService.h"
#include "Desktop/UTapLoginWidget.h"
#include "Platform/TapLoginPlatformDesktop.h"
#include "Platform/TapLoginTracker.h"
#include "Async/TaskGraphInterfaces.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Internal/Storage/TapSharedCacheFile.h"

// Exercise production HTTP delegates with controlled response ordering, without HTTP or a browser.
struct FTapDesktopLoginTestAccess
{
	static void CacheIsolation(FAutomationTestBase& Test)
	{
		const FString ClientA = TEXT("cache-test-a-") + FGuid::NewGuid().ToString();
		const FString ClientB = TEXT("cache-test-b-") + FGuid::NewGuid().ToString();
		const FString PathA = FTapLoginPlatformDesktop::LoginCachePath(ClientA);
		const FString PathB = FTapLoginPlatformDesktop::LoginCachePath(ClientB);
		Test.TestNotEqual(TEXT("Client caches are separate"), PathA, PathB);
		{
			FTapLoginPlatformDesktop A, B;
			A.ClientID = ClientA; B.ClientID = ClientB;
			A.LoginPrefs.Init(PathA); B.LoginPrefs.Init(PathB);
			FTapAccount Account;
			Account.OpenID = TEXT("account-a"); Account.AccessToken.Kid = TEXT("kid-a");
			A.CurrentAccount = Account;
			A.SaveLoginState();
			B.LoadLoginState();
			Test.TestFalse(TEXT("B cannot restore A"), B.CurrentAccount.IsSet());
			B.ClearLoginState();
			FString Json;
			FFileHelper::LoadFileToString(Json, *FPaths::Combine(FPaths::ProjectSavedDir(), PathA));
			Test.TestTrue(TEXT("B logout preserves A token"), Json.Contains(TEXT("kid-a")));
			A.CurrentAccount.Reset();
			A.LoginPrefs.Set(TEXT("client_id"), ClientB);
			A.LoadLoginState();
			Test.TestFalse(TEXT("Mismatched ownership is not restored"), A.CurrentAccount.IsSet());
			auto Lease = FTapSharedCacheFile::TryAcquire(FPaths::Combine(FPaths::ProjectSavedDir(), PathB));
			B.LoginPrefs.Init(PathB);
			Test.TestFalse(TEXT("Startup restore waits for the file lease"), B.TryRestoreCachedLogin(B.StateGeneration));
			const uint64 OldGeneration = B.StateGeneration++;
			Test.TestTrue(TEXT("Logout cancels pending restoration"), B.TryRestoreCachedLogin(OldGeneration));
			Test.TestFalse(TEXT("Cancelled restoration does not create account"), B.CurrentAccount.IsSet());
			const uint64 BeforeLogin = B.StateGeneration;
			B.BeginLogin();
			B.bLoginInProgress = false; // Login cancelled before the cache retry ticker runs.
			Test.TestTrue(TEXT("A completed or cancelled new login retires startup restoration"), B.TryRestoreCachedLogin(BeforeLogin));
			Test.TestFalse(TEXT("Fast cancellation cannot restore the previous account"), B.CurrentAccount.IsSet());
			Lease.Reset();
		}
		IFileManager::Get().DeleteDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(), FPaths::GetPath(PathA)), false, true);
		IFileManager::Get().DeleteDirectory(*FPaths::Combine(FPaths::ProjectSavedDir(), FPaths::GetPath(PathB)), false, true);
	}
	static void Reply(const FTapHttpRequestCompleteDelegate& Callback, const FTapHttpResponse& Response)
	{
		Callback.Execute(Response);
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}

	static FTapHttpResponse Success()
	{
		FTapHttpResponse Response;
		Response.bSuccess = true;
		Response.StatusCode = 200;
		Response.JsonObject = MakeShared<FJsonObject>();
		for (const TCHAR* Field : {TEXT("kid"), TEXT("access_token"), TEXT("token_type"), TEXT("mac_key"), TEXT("mac_algorithm")})
		{
			Response.JsonObject->SetStringField(Field, TEXT("test"));
		}
		return Response;
	}

	static void PollSessions(FAutomationTestBase& Test)
	{
		TSharedPtr<FTapQRCodeService> Service = MakeShared<FTapQRCodeService>();
		int32 AuthCount = 0;
		int32 StatusCount = 0;
		auto Start = [&]()
		{
			Service->StartPolling(TEXT("test"), FOnQRCodeStatusChanged::CreateLambda([&](ETapQRCodeStatus) { ++StatusCount; }),
				FOnQRCodeAuthSuccess::CreateLambda([&](const FTapTokenData&) { ++AuthCount; }));
		};
		Start();
		const auto OldReply = Service->MakePollCallback();
		Start();
		Service->bPollRequestInFlight = true;
		Service->LastPollTime = 42.0;
		const auto NewReply = Service->MakePollCallback();
		FTapHttpResponse Denied = Success();
		Denied.JsonObject->SetStringField(TEXT("error"), TEXT("access_denied"));
		Denied.JsonObject->SetNumberField(TEXT("interval"), 99);
		Reply(OldReply, Denied);
		Reply(OldReply, Success());
		Test.TestTrue(TEXT("Old denial cannot stop the new session"), Service->bPolling);
		Test.TestTrue(TEXT("Old replies cannot clear the new in-flight request"), Service->bPollRequestInFlight);
		Test.TestEqual(TEXT("Old replies cannot change poll timing"), Service->LastPollTime, 42.0);
		Test.TestEqual(TEXT("Old replies cannot change poll interval"), Service->QRCodeData.Interval, 5);
		Test.TestEqual(TEXT("Old replies cannot emit status"), StatusCount, 0);
		Test.TestEqual(TEXT("Old replies cannot authenticate"), AuthCount, 0);
		Reply(NewReply, Success());
		Reply(NewReply, Success());
		Test.TestEqual(TEXT("Current session authenticates exactly once"), AuthCount, 1);
		Test.TestFalse(TEXT("Success stops polling"), Service->bPolling);

		Start();
		const auto ClosedReply = Service->MakePollCallback();
		Service->StopPolling();
		Reply(ClosedReply, Success());
		Test.TestEqual(TEXT("Closed session cannot authenticate"), AuthCount, 1);

		Service->StartPolling(TEXT("test"), FOnQRCodeStatusChanged::CreateLambda([&](ETapQRCodeStatus) { Start(); }),
			FOnQRCodeAuthSuccess::CreateLambda([&](const FTapTokenData&) { ++AuthCount; }));
		Reply(Service->MakePollCallback(), Success());
		Test.TestTrue(TEXT("Reentrant status callback can start a new session"), Service->bPolling);
		Test.TestEqual(TEXT("Reentrant restart cannot receive the previous token"), AuthCount, 1);
		Service->StopPolling();
	}

	static void QRResponseLifetime(FAutomationTestBase& Test)
	{
		FTapHttpResponse Response = Success();
		Response.JsonObject->SetStringField(TEXT("device_code"), TEXT("fresh"));
		Response.JsonObject->SetStringField(TEXT("qrcode_url"), TEXT("https://example.test/qr"));
		Response.JsonObject->SetNumberField(TEXT("expires_in"), 60);
		Response.JsonObject->SetNumberField(TEXT("interval"), 5);
		int32 ReadyCount = 0;
		const auto OnReady = FOnQRCodeReady::CreateLambda([&](bool, const FTapQRCodeData&) { ++ReadyCount; });
		TSharedPtr<FTapQRCodeService> Service = MakeShared<FTapQRCodeService>();
		const auto OldReply = Service->MakeQRCodeCallback(OnReady);
		Service->StopPolling();
		Reply(OldReply, Response);
		Test.TestEqual(TEXT("Cancelled QR reply is ignored"), ReadyCount, 0);
		Test.TestTrue(TEXT("Cancelled QR reply cannot overwrite device code"), Service->QRCodeData.DeviceCode.IsEmpty());
		Reply(Service->MakeQRCodeCallback(OnReady), Response);
		Test.TestEqual(TEXT("Current QR reply is delivered"), ReadyCount, 1);
		Test.TestEqual(TEXT("Current device code is stored"), Service->QRCodeData.DeviceCode, FString(TEXT("fresh")));
		const auto DestroyedReply = Service->MakeQRCodeCallback(OnReady);
		Service.Reset();
		Reply(DestroyedReply, Response);
		Test.TestEqual(TEXT("Destroyed service is not accessed"), ReadyCount, 1);
	}

	static void WidgetFlow(FAutomationTestBase& Test)
	{
		UTapLoginWidget* Widget = NewObject<UTapLoginWidget>();
		Widget->AddToRoot();
		Widget->QRCodeImage = NewObject<UImage>(Widget);
		Widget->RefreshBtn = NewObject<UButton>(Widget);
		Widget->QRService = MakeShared<FTapQRCodeService>();
		// This suite tests login flow and widget visibility, not GPU/Slate texture ownership.
		Widget->CreateQRBrushForTest = [](const FString&, int32) { return TSharedPtr<FSlateDynamicImageBrush>(); };
		int32 BrowserCount = 0;
		Widget->OpenBrowserForTest = [&](const FString& URL)
		{
			++BrowserCount;
			Test.TestEqual(TEXT("Browser opens the current QR URL"), URL, FString(TEXT("https://example.test/qr")));
		};
		FTapQRCodeData Data;
		Data.DeviceCode = TEXT("current");
		Data.QRCodeUrl = TEXT("https://example.test/qr");
		Data.ExpiresIn = 60;
		// Model the outstanding initial request; use the real button and ready handlers.
		Widget->bQRRequestInFlight = true;
		Widget->OnWebLoginClicked();
		Widget->OnWebLoginClicked();
		Test.TestTrue(TEXT("Loading click is queued"), Widget->bOpenBrowserWhenQRReady);
		Test.TestEqual(TEXT("Loading click does not open the browser early"), BrowserCount, 0);
		Widget->OnQRCodeReady(Widget->QRRequestGeneration, true, Data);
		Test.TestEqual(TEXT("Queued duplicate clicks open one browser"), BrowserCount, 1);
		Test.TestTrue(TEXT("Queued click marks a browser session"), Widget->bWebLoginSession);
		Test.TestFalse(TEXT("Queued click is consumed"), Widget->bOpenBrowserWhenQRReady);

		Widget->QRService->StopPolling();
		Widget->bQRRequestInFlight = true;
		Widget->bWebLoginSession = false;
		Widget->OnWebLoginClicked();
		Widget->OnQRCodeReady(Widget->QRRequestGeneration, false, Data);
		Test.TestEqual(TEXT("Failed refresh hides obsolete QR"), Widget->QRCodeImage->GetVisibility(), ESlateVisibility::Hidden);
		Test.TestEqual(TEXT("Failed refresh offers retry"), Widget->RefreshBtn->GetVisibility(), ESlateVisibility::Visible);
		Test.TestFalse(TEXT("Failure clears pending browser intent"), Widget->bOpenBrowserWhenQRReady);
		Test.TestFalse(TEXT("Failure unlocks requests"), Widget->bQRRequestInFlight);
		Widget->OnQRCodeReady(Widget->QRRequestGeneration, true, Data);
		Test.TestEqual(TEXT("Scan retry does not launch browser"), BrowserCount, 1);
		Test.TestFalse(TEXT("Scan retry keeps code classification"), Widget->bWebLoginSession);
		Test.TestEqual(TEXT("Successful retry hides retry button"), Widget->RefreshBtn->GetVisibility(), ESlateVisibility::Collapsed);

		Widget->QRService->StopPolling();
		Widget->bQRRequestInFlight = true;
		Widget->OnWebLoginClicked();
		const uint64 ClosedRequest = Widget->QRRequestGeneration;
		Widget->OnCloseClicked();
		Widget->OnQRCodeReady(ClosedRequest, true, Data);
		Test.TestEqual(TEXT("Closing discards queued browser launch"), BrowserCount, 1);
		Test.TestFalse(TEXT("Closing releases service"), Widget->QRService.IsValid());
		Widget->NativeDestruct();
		Widget->RemoveFromRoot();
	}

	static void LoginType(FAutomationTestBase& Test)
	{
		FTapLoginPlatformDesktop Platform;
		UTapLoginWidget* Widget = NewObject<UTapLoginWidget>();
		Widget->Init(TEXT("test"), {}, FOnLoginPanelClose(),
			FOnLoginPanelAuthSuccess::CreateRaw(&Platform, &FTapLoginPlatformDesktop::HandlePanelAuthSuccess));
		int32 SuccessCount = 0;
		for (bool bBrowser : {true, false})
		{
			Platform.PendingAccessTokenCallback = [&](const FTapAccessTokenResult& Result)
			{
				if (Result.bSuccess) ++SuccessCount;
			};
			Widget->bWebLoginSession = bBrowser;
			Widget->OnQRCodeAuthSuccess(FTapTokenData());
			Test.TestEqual(TEXT("Widget login type reaches platform tracking"), Platform.CurrentLoginType,
				bBrowser ? FTapLoginTracker::LOGIN_TYPE_BROWSER : FTapLoginTracker::LOGIN_TYPE_CODE);
		}
		Test.TestEqual(TEXT("Both login paths deliver tokens"), SuccessCount, 2);
	}
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapDesktopPollSessionTest, "TapLogin.Desktop.DeviceCode.PollSessions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapDesktopPollSessionTest::RunTest(const FString&)
{
	FTapDesktopLoginTestAccess::PollSessions(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapDesktopQRResponseTest, "TapLogin.Desktop.DeviceCode.QRResponseLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapDesktopQRResponseTest::RunTest(const FString&)
{
	FTapDesktopLoginTestAccess::QRResponseLifetime(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapDesktopWidgetFlowTest, "TapLogin.Desktop.DeviceCode.WidgetFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapDesktopWidgetFlowTest::RunTest(const FString&)
{
	FTapDesktopLoginTestAccess::WidgetFlow(*this);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapDesktopLoginTypeTest, "TapLogin.Desktop.DeviceCode.LoginType",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapDesktopLoginTypeTest::RunTest(const FString&)
{
	FTapDesktopLoginTestAccess::LoginType(*this);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapDesktopLoginCacheIsolationTest, "TapLogin.Desktop.Cache.ClientIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FTapDesktopLoginCacheIsolationTest::RunTest(const FString&)
{
	FTapDesktopLoginTestAccess::CacheIsolation(*this);
	return true;
}
#endif

// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX) && WITH_DEV_AUTOMATION_TESTS

#include "Platform/TapCloudSaveDesktopToken.h"
#include "Dom/JsonObject.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapCloudSaveDesktopTokenJsonTest,
	"TapSDK.CloudSave.Desktop.TokenJson",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapCloudSaveDesktopTokenJsonTest::RunTest(const FString& /*Parameters*/)
{
	FTapAccount Account;
	Account.AccessToken.Kid = TEXT("kid_1");
	Account.AccessToken.MacKey = TEXT("mac_key_1");

	const TSharedRef<FJsonObject> Token =
		TapCloudSaveDesktopToken::MakeTokenObject(Account);
	TestEqual(TEXT("sdk_token.kid uses TapLogin token kid"),
		Token->GetStringField(TEXT("kid")), Account.AccessToken.Kid);
	TestEqual(TEXT("sdk_token.key uses TapLogin MAC key"),
		Token->GetStringField(TEXT("key")), Account.AccessToken.MacKey);
	TestTrue(TEXT("complete account has MAC credentials"),
		TapCloudSaveDesktopToken::HasCompleteMacCredentials(Account));

	FTapAccount MissingKey = Account;
	MissingKey.AccessToken.MacKey.Reset();
	TestFalse(TEXT("account missing MAC key is incomplete"),
		TapCloudSaveDesktopToken::HasCompleteMacCredentials(MissingKey));

	FTapAccount MissingKid = Account;
	MissingKid.AccessToken.Kid.Reset();
	TestFalse(TEXT("account missing token kid is incomplete"),
		TapCloudSaveDesktopToken::HasCompleteMacCredentials(MissingKid));

	const TSharedRef<FJsonObject> EmptyToken =
		TapCloudSaveDesktopToken::MakeTokenObject(TOptional<FTapAccount>());
	TestTrue(TEXT("missing account emits an empty kid"),
		EmptyToken->GetStringField(TEXT("kid")).IsEmpty());
	TestTrue(TEXT("missing account emits an empty key"),
		EmptyToken->GetStringField(TEXT("key")).IsEmpty());
	TestFalse(TEXT("missing account has no MAC credentials"),
		TapCloudSaveDesktopToken::HasCompleteMacCredentials(TOptional<FTapAccount>()));
	return true;
}

#endif // (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX) && WITH_DEV_AUTOMATION_TESTS

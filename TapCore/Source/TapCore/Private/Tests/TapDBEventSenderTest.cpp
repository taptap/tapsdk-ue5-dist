// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Internal/TapDBConstants.h"
#include "TapDBEventSenderUtils.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapDBEventSenderRequestEventTest,
	"TapDB.EventSender.RequestEvent.ExcludesRetryCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapDBEventSenderRequestEventTest::RunTest(const FString& /*Parameters*/)
{
	TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("name"), TEXT("test_event"));
	Event->SetNumberField(TEXT("_retry_count"), 2);

	TSharedPtr<FJsonObject> RequestEvent = TapDBEventSenderUtils::CreateRequestEvent(Event);
	TestTrue(TEXT("Request event is valid"), RequestEvent.IsValid());
	TestTrue(TEXT("Request event keeps regular fields"),
		RequestEvent->HasField(TEXT("name")));
	TestFalse(TEXT("Request event excludes internal retry count"),
		RequestEvent->HasField(TEXT("_retry_count")));
	TestTrue(TEXT("Original event keeps retry count"),
		Event->HasField(TEXT("_retry_count")));
	TestEqual(TEXT("Original retry count is unchanged"),
		Event->GetIntegerField(TEXT("_retry_count")), 2);

	FString Serialized;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
	TestTrue(TEXT("Request event serializes"),
		FJsonSerializer::Serialize(RequestEvent.ToSharedRef(), Writer));
	TestFalse(TEXT("Serialized request event excludes retry count"),
		Serialized.Contains(TEXT("_retry_count")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapDBEventSenderRetryBoundaryTest,
	"TapDB.EventSender.Retry.BoundedAtThree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FTapDBEventSenderRetryBoundaryTest::RunTest(const FString& /*Parameters*/)
{
	TSharedPtr<FJsonObject> Event = MakeShared<FJsonObject>();
	Event->SetStringField(TEXT("name"), TEXT("game_start"));

	for (int32 Retry = 1; Retry <= TapDBConstants::MAX_RETRY; ++Retry)
	{
		TestTrue(TEXT("Immediate 5xx retry stays within the per-send limit"),
			TapDBEventSenderUtils::PrepareRetry(Event));
		TestEqual(TEXT("Retry count advances"),
			Event->GetIntegerField(TEXT("_retry_count")), Retry);
	}

	TestFalse(TEXT("Immediate 5xx retries stop after three"),
		TapDBEventSenderUtils::PrepareRetry(Event));
	TestEqual(TEXT("Exhausted immediate retries do not mutate retry count"),
		Event->GetIntegerField(TEXT("_retry_count")), TapDBConstants::MAX_RETRY);

	TSharedPtr<FJsonObject> DeviceLogin = MakeShared<FJsonObject>();
	DeviceLogin->SetStringField(TEXT("name"), TapDBConstants::EVENT_DEVICE_LOGIN);
	TestTrue(TEXT("device_login can retry like other events"),
		TapDBEventSenderUtils::PrepareRetry(DeviceLogin));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

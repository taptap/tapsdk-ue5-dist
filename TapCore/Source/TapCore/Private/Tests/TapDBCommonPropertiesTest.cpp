// Copyright TapTap. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Internal/TapDBEventTracker.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapDBCommonPropertiesTypesTest,
	"TapDB.EventTracker.CommonProperties.PreservesJsonTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapDBCommonPropertiesTypesTest::RunTest(const FString& /*Parameters*/)
{
	// Local tracker: no SDK init, timers, persistence or network requests.
	FTapDBEventTracker Tracker;
	Tracker.bIsInitialized = true;
	Tracker.AddCommon(TEXT("{\"server\":\"asia\",\"level\":3,\"vip\":true,\"tags\":[\"new\"],\"profile\":{\"rank\":2}}"));
	TSharedPtr<FJsonObject> Event = Tracker.BuildTrackEvent(TEXT("test_event"), nullptr, false);
	const TSharedPtr<FJsonObject> Props = Event->GetObjectField(TEXT("properties"));

	FString Server;
	TestTrue(TEXT("String property survives"), Props->TryGetStringField(TEXT("server"), Server));
	TestEqual(TEXT("String value is unchanged"), Server, FString(TEXT("asia")));

	double Level = 0;
	TestTrue(TEXT("Number property survives as a number"), Props->TryGetNumberField(TEXT("level"), Level));
	TestEqual(TEXT("Number value is unchanged"), Level, 3.0);

	bool bVip = false;
	TestTrue(TEXT("Boolean property survives as a boolean"), Props->TryGetBoolField(TEXT("vip"), bVip));
	TestTrue(TEXT("Boolean value is unchanged"), bVip);

	const TArray<TSharedPtr<FJsonValue>>* Tags = nullptr;
	TestTrue(TEXT("Array property survives as an array"), Props->TryGetArrayField(TEXT("tags"), Tags));
	const TSharedPtr<FJsonObject>* Profile = nullptr;
	TestTrue(TEXT("Object property survives as an object"), Props->TryGetObjectField(TEXT("profile"), Profile));

	// Avoid production shutdown: this local tracker never initialized its components.
	Tracker.bIsInitialized = false;
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

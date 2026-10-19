// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "TapCloudSaveTypes.h"
#include "JsonObjectConverter.h"

/**
 * Round-trips FTapArchiveMetadata via UE5's reflection-based
 * FJsonObjectConverter to lock down BP-facing field naming and Unicode
 * preservation. Any rename or UPROPERTY drift will fail this test.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapArchiveMetadataJsonRoundTrip,
	"TapSDK.CloudSave.Types.ArchiveMetadata.RoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapArchiveMetadataJsonRoundTrip::RunTest(const FString& /*Parameters*/)
{
	FTapArchiveMetadata In;
	In.Name = TEXT("Slot1");
	In.Summary = TEXT("摘要中文");                    // verify UTF-8 preservation
	In.Extra = TEXT("{\"k\":1}");                     // JSON-in-string survives escape round-trip
	In.Playtime = 1234;

	FString Out;
	TestTrue(TEXT("UStructToJsonObjectString succeeds"),
		FJsonObjectConverter::UStructToJsonObjectString(In, Out));
	TestTrue(TEXT("Serialized JSON contains 'Slot1'"),  Out.Contains(TEXT("Slot1")));
	TestTrue(TEXT("Serialized JSON preserves Chinese"), Out.Contains(TEXT("摘要中文")));

	FTapArchiveMetadata Back;
	TestTrue(TEXT("JsonObjectStringToUStruct succeeds"),
		FJsonObjectConverter::JsonObjectStringToUStruct(Out, &Back, 0, 0));

	TestEqual(TEXT("Name preserved"),     Back.Name,     In.Name);
	TestEqual(TEXT("Summary preserved"),  Back.Summary,  In.Summary);
	TestEqual(TEXT("Extra preserved"),    Back.Extra,    In.Extra);
	TestEqual(TEXT("Playtime preserved"), Back.Playtime, In.Playtime);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

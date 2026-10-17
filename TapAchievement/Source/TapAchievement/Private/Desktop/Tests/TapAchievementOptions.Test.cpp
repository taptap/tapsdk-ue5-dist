// Copyright TapTap. All Rights Reserved.

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/TapSdkOptionSerializerRegistry.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TapAchievementOptions.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAchievementOptionsSerializationTest,
    "TapSDK.Achievement.Options.SerializeForNativeInit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapAchievementOptionsSerializationTest::RunTest(const FString&)
{
    FTapAchievementOptions AchievementOptions;
    AchievementOptions.bEnableToast = false;

    FTapTapSdkOptions Config;
    FTapSdkConfigState::SetModuleOptions(
        Config,
        {FTapSdkOption::Make<FTapAchievementOptions>(AchievementOptions)});

    TArray<FTapSerializedSdkModuleOption> SerializedOptions;
    FTapSdkOptionSerializerRegistry::Get().SerializeOptions(Config, SerializedOptions);
    TestEqual(TEXT("One Achievement option should be serialized"), SerializedOptions.Num(), 1);
    if (SerializedOptions.Num() != 1)
    {
        return false;
    }

    TestEqual(TEXT("iOS native option class should stay internal and deterministic"),
        SerializedOptions[0].NativeClassName, FString(TEXT("TapTapAchievementOptions")));

    TSharedPtr<FJsonObject> Json;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SerializedOptions[0].Json);
    TestTrue(TEXT("Serialized Achievement option should be valid JSON"),
        FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid());
    if (Json.IsValid())
    {
        TestFalse(TEXT("enableToast should be forwarded"), Json->GetBoolField(TEXT("enableToast")));
    }
    return true;
}

#endif

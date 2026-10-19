// Copyright TapTap. All Rights Reserved.

#include "Internal/TapSdkOptionSerializerRegistry.h"

#include "Dom/JsonObject.h"
#include "Internal/TapSdkConfigState.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TapCoreModule.h"

FTapSdkOptionSerializerRegistry& FTapSdkOptionSerializerRegistry::Get()
{
	static FTapSdkOptionSerializerRegistry Instance;
	return Instance;
}

void FTapSdkOptionSerializerRegistry::RegisterSerializer(
	const UScriptStruct* OptionType,
	const FString& NativeClassName,
	FTapSdkModuleOptionSerializer Serializer)
{
	if (!OptionType || NativeClassName.IsEmpty() || !Serializer)
	{
		return;
	}

	FScopeLock Lock(&RegistryLock);
	Serializers.Add(OptionType, {NativeClassName, MoveTemp(Serializer)});
}

void FTapSdkOptionSerializerRegistry::UnregisterSerializer(const UScriptStruct* OptionType)
{
	FScopeLock Lock(&RegistryLock);
	Serializers.Remove(OptionType);
}

void FTapSdkOptionSerializerRegistry::SerializeOptions(
	const FTapTapSdkOptions& Config,
	TArray<FTapSerializedSdkModuleOption>& OutOptions) const
{
	OutOptions.Reset();

	for (const FTapSdkOption& Option : FTapSdkConfigState::GetModuleOptions(Config))
	{
		const UScriptStruct* OptionType = Option.GetScriptStruct();
		if (OptionType == FTapTapEventOptions::StaticStruct())
		{
			// TapTap Event belongs to TapCore and is serialized by the platform bridge itself.
			continue;
		}

		FSerializerEntry Entry;
		{
			FScopeLock Lock(&RegistryLock);
			const FSerializerEntry* RegisteredEntry = Serializers.Find(OptionType);
			if (!RegisteredEntry)
			{
				UE_LOG(LogTapCore, Warning, TEXT("No native serializer registered for SDK option: %s"),
					OptionType ? *OptionType->GetName() : TEXT("<null>"));
				continue;
			}
			Entry = *RegisteredEntry;
		}

		const TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		Entry.Serializer(Option, JsonObject);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(JsonObject, Writer);
		OutOptions.Add({Entry.NativeClassName, MoveTemp(Json)});
	}
}

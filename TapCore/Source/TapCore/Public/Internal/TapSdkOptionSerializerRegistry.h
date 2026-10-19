// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"

class FJsonObject;

/** Serialized feature-module option consumed by the native Android/iOS initialization bridges. */
struct TAPCORE_API FTapSerializedSdkModuleOption
{
	FString NativeClassName;
	FString Json;
};

using FTapSdkModuleOptionSerializer = TFunction<void(const FTapSdkOption&, const TSharedRef<FJsonObject>&)>;

/** Internal registry that lets feature modules serialize their own initialization options. */
class TAPCORE_API FTapSdkOptionSerializerRegistry
{
public:
	static FTapSdkOptionSerializerRegistry& Get();

	void RegisterSerializer(
		const UScriptStruct* OptionType,
		const FString& NativeClassName,
		FTapSdkModuleOptionSerializer Serializer);
	void UnregisterSerializer(const UScriptStruct* OptionType);
	void SerializeOptions(const FTapTapSdkOptions& Config, TArray<FTapSerializedSdkModuleOption>& OutOptions) const;

private:
	struct FSerializerEntry
	{
		FString NativeClassName;
		FTapSdkModuleOptionSerializer Serializer;
	};

	mutable FCriticalSection RegistryLock;
	TMap<const UScriptStruct*, FSerializerEntry> Serializers;
};

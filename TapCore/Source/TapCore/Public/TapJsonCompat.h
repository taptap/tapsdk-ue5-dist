// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Helpers for FJsonObject key type differences:
 *   UE 5.7 — TMap<FString, TSharedPtr<FJsonValue>>
 *   UE 5.8 — TMap<UE::FSharedString, TSharedPtr<FJsonValue>>
 *
 * Do not define UE_JSONOBJECT_LEGACY_STRING_KEYS: the engine Json module is already
 * compiled against the default key type, so flipping the macro would be an ABI mismatch.
 */
namespace TapJson
{
	FORCEINLINE FString KeyString(const FString& Key)
	{
		return Key;
	}

	template <typename KeyType>
	FORCEINLINE FString KeyString(const KeyType& Key)
	{
		return FString(*Key);
	}

	FORCEINLINE TSharedPtr<FJsonValue> FindField(const TSharedPtr<FJsonObject>& Root, FStringView FieldName)
	{
		return Root.IsValid() ? Root->TryGetField(FieldName) : TSharedPtr<FJsonValue>();
	}

	FORCEINLINE TSharedPtr<FJsonValue> FindField(const FJsonObject& Root, FStringView FieldName)
	{
		return Root.TryGetField(FieldName);
	}
}

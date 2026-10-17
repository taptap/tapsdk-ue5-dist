// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * JSON file-based key-value persistence storage.
 * Thread-safe. Generic — not tied to any specific module.
 *
 * Usage:
 *   FTapPrefs Prefs;
 *   Prefs.Init(TEXT("MyModule/my_storage.json"));  // relative to ProjectSavedDir
 */
class TAPCORE_API FTapPrefs
{
public:
	FTapPrefs();
	~FTapPrefs();

	/** Initialize with a path relative to ProjectSavedDir. Creates parent directories if needed. */
	void Init(const FString& RelativePath);

	FString GetString(const FString& Key, const FString& Default = TEXT("")) const;
	int64 GetInt64(const FString& Key, int64 Default = 0) const;
	void Set(const FString& Key, const FString& Value);
	void SetInt64(const FString& Key, int64 Value);
	bool Remove(const FString& Key);
	void Clear();
	void Save();

private:
	void Load();

	TSharedPtr<FJsonObject> Data;
	FString FilePath;
	mutable FCriticalSection Lock;
};

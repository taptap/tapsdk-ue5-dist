// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"

/**
 * JSON file-based key-value persistence storage.
 * Thread-safe. Generic — not tied to any specific module.
 *
 * Usage:
 *   FTapPrefs Prefs;
 *   Prefs.Init(TEXT("MyModule/my_storage.json"));  // relative to ProjectSavedDir
 */
class TAPCORE_API FTapPrefs : private FRunnable
{
public:
	FTapPrefs();
	~FTapPrefs();

	/** Initialize with a path relative to ProjectSavedDir. Creates parent directories if needed. */
	void Init(const FString& RelativePath, bool bPrivateFile = false, const FString& MachineFingerprint = TEXT(""));
	bool IsReady() const;
	/** True only when loading and all pending writes have completed. */
	bool IsPersisted();
	const FString& GetFilePath() const { return FilePath; }
	bool WasInvalidated() const;

	FString GetString(const FString& Key, const FString& Default = TEXT("")) const;
	int64 GetInt64(const FString& Key, int64 Default = 0) const;
	void Set(const FString& Key, const FString& Value);
	void SetInt64(const FString& Key, int64 Value);
	bool Remove(const FString& Key);
	void Clear();
	void Save();

private:
	virtual bool Init() override { return true; }
	bool Load();
	void Retry();
	void StopWorker();
	virtual uint32 Run() override;

	TSharedPtr<FJsonObject> Data;
	FString FilePath;
	mutable FCriticalSection Lock;
	TSet<FString> DirtyKeys;
	TSet<FString> RemovedKeys;
	bool bClearPending = false;
	bool bReady = false;
	bool bPrivate = false;
	FString Fingerprint;
	bool bInvalidated = false;
	FCriticalSection SaveLock;
	FCriticalSection WorkerLock;
	FEvent* Wake = nullptr;
	FRunnableThread* Worker = nullptr;
	TAtomic<bool> bStopping{false};
};

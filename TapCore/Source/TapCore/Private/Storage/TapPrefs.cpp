// Copyright TapTap. All Rights Reserved.

#include "Internal/Storage/TapPrefs.h"
#include "Internal/Storage/TapSharedCacheFile.h"
#include "TapCoreModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ScopeExit.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"

FTapPrefs::FTapPrefs()
{
	Data = MakeShared<FJsonObject>();
}

FTapPrefs::~FTapPrefs()
{
	StopWorker();
	Save(); // One final non-waiting attempt; a destroyed owner cannot run a retry worker.
}

void FTapPrefs::StopWorker()
{
	bStopping = true;
	if (Wake) Wake->Trigger();
	if (Worker) { Worker->WaitForCompletion(); delete Worker; Worker = nullptr; }
	if (Wake) { FPlatformProcess::ReturnSynchEventToPool(Wake); Wake = nullptr; }
}

void FTapPrefs::Retry()
{
	FScopeLock Guard(&WorkerLock);
	if (bStopping) return;
	if (!Worker)
	{
		Wake = FPlatformProcess::GetSynchEventFromPool(false);
		Worker = FRunnableThread::Create(this, TEXT("TapPrefsRetry"));
		if (!Worker)
		{
			FPlatformProcess::ReturnSynchEventToPool(Wake);
			Wake = nullptr;
			UE_LOG(LogTapBasic, Warning, TEXT("[TapPrefs] Could not start persistence retry worker"));
		}
	}
}

uint32 FTapPrefs::Run()
{
	while (!bStopping)
	{
		Wake->Wait(1000);
		if (!bStopping) Save();
	}
	return 0;
}

bool FTapPrefs::IsReady() const
{
	FScopeLock Guard(&Lock);
	return bReady;
}

bool FTapPrefs::IsPersisted()
{
	if (!SaveLock.TryLock()) return false;
	ON_SCOPE_EXIT { SaveLock.Unlock(); };
	FScopeLock Guard(&Lock);
	return bReady && DirtyKeys.IsEmpty() && RemovedKeys.IsEmpty() && !bClearPending;
}

bool FTapPrefs::WasInvalidated() const
{
	FScopeLock Guard(&Lock);
	return bInvalidated;
}

void FTapPrefs::Init(const FString& RelativePath, bool bPrivateFile, const FString& MachineFingerprint)
{
	StopWorker();
	bStopping = false;
	FScopeLock Persistence(&SaveLock);
	bPrivate = bPrivateFile;
	Fingerprint = MachineFingerprint;
	FilePath = FPaths::IsRelative(RelativePath) ? FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath) : RelativePath;
	{
		FScopeLock Guard(&Lock);
		bInvalidated = false;
		Data = MakeShared<FJsonObject>();
		bReady = false;
		DirtyKeys.Empty();
		RemovedKeys.Empty();
		bClearPending = false;
	}

	// Ensure parent directory exists
	FString Dir = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}

	if (!Load()) Retry();
}

FString FTapPrefs::GetString(const FString& Key, const FString& Default) const
{
	FScopeLock ScopeLock(&Lock);
	FString Value;
	if (Data.IsValid() && Data->TryGetStringField(Key, Value))
	{
		return Value;
	}
	return Default;
}

int64 FTapPrefs::GetInt64(const FString& Key, int64 Default) const
{
	FScopeLock ScopeLock(&Lock);
	if (Data.IsValid() && Data->HasField(Key))
	{
		return static_cast<int64>(Data->GetNumberField(Key));
	}
	return Default;
}

void FTapPrefs::Set(const FString& Key, const FString& Value)
{
	FScopeLock ScopeLock(&Lock);
	if (Data.IsValid())
	{
		Data->SetStringField(Key, Value);
		DirtyKeys.Add(Key);
		RemovedKeys.Remove(Key);
	}
}

void FTapPrefs::SetInt64(const FString& Key, int64 Value)
{
	FScopeLock ScopeLock(&Lock);
	if (Data.IsValid())
	{
		Data->SetNumberField(Key, static_cast<double>(Value));
		DirtyKeys.Add(Key);
		RemovedKeys.Remove(Key);
	}
}

bool FTapPrefs::Remove(const FString& Key)
{
	FScopeLock ScopeLock(&Lock);
	const bool bExisted = Data.IsValid() && Data->HasField(Key);
	if (Data.IsValid())
	{
		Data->RemoveField(Key);
		DirtyKeys.Remove(Key);
		RemovedKeys.Add(Key);
	}
	return bExisted;
}

void FTapPrefs::Clear()
{
	FScopeLock ScopeLock(&Lock);
	Data = MakeShared<FJsonObject>();
	DirtyKeys.Empty();
	RemovedKeys.Empty();
	bClearPending = true;
}

void FTapPrefs::Save()
{
	// One writer per object; foreground mutations never wait for file IO.
	if (!SaveLock.TryLock()) { Retry(); return; }
	ON_SCOPE_EXIT { SaveLock.Unlock(); };
	if (FilePath.IsEmpty()) return;
	if (!IsReady() && !Load()) { Retry(); return; }

	TMap<FString, TSharedPtr<FJsonValue>> Updates;
	TSet<FString> Removals;
	bool bClear;
	{
		FScopeLock Guard(&Lock);
		if (DirtyKeys.IsEmpty() && RemovedKeys.IsEmpty() && !bClearPending) return;
		for (const FString& Key : DirtyKeys) Updates.Add(Key, Data->TryGetField(Key));
		Removals = MoveTemp(RemovedKeys);
		DirtyKeys.Empty();
		bClear = bClearPending;
		bClearPending = false;
	}
	bool bSaved = false;
	ON_SCOPE_EXIT
	{
		if (!bSaved)
		{
			FScopeLock Guard(&Lock);
			// Retry current values, preserving mutations made during IO (including Clear).
			if (!bClearPending)
			{
				for (const auto& Pair : Updates)
					if (Data->HasField(Pair.Key)) DirtyKeys.Add(Pair.Key); else RemovedKeys.Add(Pair.Key);
				for (const FString& Key : Removals)
					if (Data->HasField(Key)) DirtyKeys.Add(Key); else RemovedKeys.Add(Key);
				bClearPending = bClear;
			}
			Retry();
		}
	};
	auto Lease = bPrivate ? TUniquePtr<FTapSharedCacheFile::FLease>() : FTapSharedCacheFile::TryAcquire(FilePath);
	if (!bPrivate && !Lease) return;
	auto Latest = MakeShared<FJsonObject>();
	if (!bClear)
	{
		FString Current;
		if (FFileHelper::LoadFileToString(Current, *FilePath))
		{
			TSharedPtr<FJsonObject> Parsed;
			if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Current), Parsed) || !Parsed) return;
			Latest = Parsed.ToSharedRef();
		}
		else if (IFileManager::Get().FileExists(*FilePath)) return;
	}
	for (const FString& Key : Removals) Latest->RemoveField(Key);
	for (const auto& Pair : Updates) Latest->SetField(Pair.Key, Pair.Value);
	FString Output;
	FJsonSerializer::Serialize(Latest, TJsonWriterFactory<>::Create(&Output));
	if (!FTapSharedCacheFile::WriteUnderLock(FilePath, Output)) return;
	bSaved = true;
	{
		FScopeLock Guard(&Lock);
		if (!bClearPending)
		{
			// Keep local edits that arrived while the snapshot was being written.
			for (const FString& Key : RemovedKeys) Latest->RemoveField(Key);
			for (const FString& Key : DirtyKeys) Latest->SetField(Key, Data->TryGetField(Key));
			Data = Latest;
		}
	}
}

bool FTapPrefs::Load()
{
	auto Lease = bPrivate ? TUniquePtr<FTapSharedCacheFile::FLease>() : FTapSharedCacheFile::TryAcquire(FilePath);
	if (!bPrivate && !Lease) return false;
	TSharedPtr<FJsonObject> Loaded = MakeShared<FJsonObject>();
	FString Json;
	if (FFileHelper::LoadFileToString(Json, *FilePath))
	{
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Loaded) || !Loaded)
		{
			// Optional preferences may be rebuilt, but only while owning the file.
			if (!FTapSharedCacheFile::WriteUnderLock(FilePath, TEXT("{}"))) return false;
			Loaded = MakeShared<FJsonObject>();
		}
	}
	else if (IFileManager::Get().FileExists(*FilePath)) return false;
	bool bMismatch = false;
	if (!Fingerprint.IsEmpty())
	{
		FString Stored;
		Loaded->TryGetStringField(TEXT("tap_machine_fp"), Stored);
		bMismatch = !Stored.IsEmpty() && Stored != Fingerprint;
		if (Stored != Fingerprint)
		{
			if (bMismatch) Loaded = MakeShared<FJsonObject>();
			Loaded->SetStringField(TEXT("tap_machine_fp"), Fingerprint);
			FString BoundJson;
			FJsonSerializer::Serialize(Loaded.ToSharedRef(), TJsonWriterFactory<>::Create(&BoundJson));
			if (!FTapSharedCacheFile::WriteUnderLock(FilePath, BoundJson)) return false;
		}
	}
	FScopeLock Guard(&Lock);
	bInvalidated |= bMismatch;
	if (!bClearPending)
	{
		for (const FString& Key : RemovedKeys) Loaded->RemoveField(Key);
		for (const FString& Key : DirtyKeys) Loaded->SetField(Key, Data->TryGetField(Key));
		Data = Loaded;
	}
	bReady = true;
	return true;
}

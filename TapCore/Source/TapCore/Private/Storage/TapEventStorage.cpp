// Copyright TapTap. All Rights Reserved.
#include "TapEventStorage.h"
#include "TapProcessIdentity.h"
#include "Internal/Storage/TapSharedCacheFile.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/TapDBConstants.h"
#include "TapCoreModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "TapJsonCompat.h"
#include "Async/Async.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include <filesystem>

namespace
{
std::filesystem::path Native(const FString& Path)
{
	return std::filesystem::u8path(TCHAR_TO_UTF8(*Path));
}
// Never follow links/reparse points while discovering, reading or deleting history.
bool Safe(const FString& Path, bool bDirectory)
{
#if PLATFORM_WINDOWS
	const DWORD Attributes = GetFileAttributesW(*Path);
	return Attributes != INVALID_FILE_ATTRIBUTES && !(Attributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
		!!(Attributes & FILE_ATTRIBUTE_DIRECTORY) == bDirectory;
#else
	std::error_code Error;
	const auto Status = std::filesystem::symlink_status(Native(Path), Error);
	return !Error && (bDirectory ? std::filesystem::is_directory(Status) : std::filesystem::is_regular_file(Status));
#endif
}
bool ReadObject(const FString& Path, TSharedPtr<FJsonObject>& Value)
{
	FString Json;
	return Safe(Path, false) && FFileHelper::LoadFileToString(Json, *Path) &&
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Value) && Value.IsValid();
}
bool WriteObject(const FString& Path, const TSharedRef<FJsonObject>& Value)
{
	FString Json;
	return FJsonSerializer::Serialize(Value, TJsonWriterFactory<>::Create(&Json)) &&
		FTapSharedCacheFile::WriteUnderLock(Path, Json);
}
FString Hash(const FString& Value)
{
	FTCHARToUTF8 Utf8(*Value);
	FMD5 Digest;
	Digest.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
	uint8 Bytes[16]; Digest.Final(Bytes);
	FString Hex;
	for (uint8 Byte : Bytes) Hex += FString::Printf(TEXT("%02x"), Byte);
	return Hex;
}
bool NewOwner(const FString& Scope, TSharedRef<FJsonObject> Owner)
{
	const uint32 Pid = FPlatformProcess::GetCurrentProcessId();
	FString Started;
	bool bExited;
	if (!TapProcessIdentity::Query(Pid, Started, bExited) || bExited) return false;
	Owner->SetNumberField(TEXT("version"), 1);
	Owner->SetStringField(TEXT("scope"), Scope);
	Owner->SetNumberField(TEXT("pid"), Pid);
	Owner->SetStringField(TEXT("started"), Started);
	return true;
}
// The only recognized temporary files are those emitted by TapSharedCacheFileNative.
bool CacheTemporary(const FString& Name)
{
	FString Tail;
	if (Name.StartsWith(TEXT("prefs.json.tmp."))) Tail = Name.Mid(15);
	else if (Name.StartsWith(TEXT("events.json.tmp."))) Tail = Name.Mid(16);
	else return false;
	FString Pid, Sequence;
	if (!Tail.Split(TEXT("."), &Pid, &Sequence) || Pid.IsEmpty() || Sequence.IsEmpty()) return false;
	for (TCHAR C : Pid + Sequence) if (C < '0' || C > '9') return false;
	return true;
}
}

FTapEventStorage& FTapEventStorage::Get()
{
	static FTapEventStorage Storage;
	return Storage;
}

bool FTapEventStorage::Prepare(const FString& Client, ETapRegion Region)
{
#if PLATFORM_WINDOWS && !WITH_EDITOR
	bool bPrimary;
	const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
	if (!TapProcessIdentity::Register(Hash(Saved.ToUpper()), bPrimary) || !Initialize(Saved, Client, Region, bPrimary))
	{
		if (!bWarnedUnavailable)
			UE_LOG(LogTapDB, Warning, TEXT("Statistics storage unavailable; skipping statistics startup without shared-file fallback"));
		bWarnedUnavailable = true;
		return false;
	}
	bWarnedUnavailable = false;
#endif
	return true;
}

bool FTapEventStorage::Initialize(const FString& SavedRoot, const FString& Client, ETapRegion Region, bool bPrimary)
{
	const FString Fingerprint = FTapMachineBoundPrefs::ComputeFingerprint();
	const FString NewRoot = FPaths::Combine(FPaths::ConvertRelativePathToFull(SavedRoot), TEXT("TapDB/process"));
	const FString RouteKey = Hash(Client + TEXT("\n") + FString::FromInt(static_cast<int32>(Region)));
	const FString NewScope = Hash(Client + TEXT("\n") + FString::FromInt(static_cast<int32>(Region)) + TEXT("\n") + Fingerprint);
	if (bPrepared) return Root == NewRoot && Scope == NewScope && bIsolated == !bPrimary;
#if PLATFORM_WINDOWS
	// Include recovery directory and atomic-write temporary suffix in the path budget.
	if (NewRoot.Len() + 110 >= 260) return false;
#endif
	if (!IFileManager::Get().MakeDirectory(*NewRoot, true) || !Safe(NewRoot, true)) return false;
	const FString LegacyPrefs = FPaths::Combine(SavedRoot, TEXT("TapDB"), FString::Printf(TEXT("tapdb_storage_%s.json"), *Client));
	const FString LegacyEvents = FPaths::Combine(SavedRoot, TEXT("TapDB"), FString::Printf(TEXT("tapdb_events_%s.json"), *Client));
	// Serialize only bootstrap metadata. Never wait on the game thread.
	auto Lease = FTapSharedCacheFile::TryAcquire(FPaths::Combine(NewRoot, TEXT("prepare")));
	if (!Lease) return false;
	const FString RoutePath = FPaths::Combine(NewRoot, TEXT("legacy-") + Hash(Client) + TEXT(".json"));
	TSharedPtr<FJsonObject> Route;
	FString RouteScope;
	bool bUseLegacy = true;
	if (IFileManager::Get().FileExists(*RoutePath))
	{
		if (!ReadObject(RoutePath, Route) || !Route->TryGetStringField(TEXT("scope"), RouteScope)) return false;
		bUseLegacy = RouteScope == RouteKey;
		if (!bUseLegacy && bPrimary) return false;
	}
	else
	{
		Route = MakeShared<FJsonObject>(); Route->SetStringField(TEXT("scope"), RouteKey);
		if (!WriteObject(RoutePath, Route.ToSharedRef())) return false;
	}
	const FString IdentityPath = FPaths::Combine(NewRoot, TEXT("identity-") + NewScope + TEXT(".json"));
	TSharedPtr<FJsonObject> Identity;
	if (IFileManager::Get().FileExists(*IdentityPath))
	{
		if (!ReadObject(IdentityPath, Identity)) return false;
	}
	else
	{
		Identity = MakeShared<FJsonObject>();
		TSharedPtr<FJsonObject> Previous;
		if (bUseLegacy && IFileManager::Get().FileExists(*LegacyPrefs))
		{
			auto LegacyLease = FTapSharedCacheFile::TryAcquire(LegacyPrefs);
			if (!LegacyLease || !ReadObject(LegacyPrefs, Previous)) return false;
			FString OldFingerprint;
			Previous->TryGetStringField(TEXT("tap_machine_fp"), OldFingerprint);
			if (!OldFingerprint.IsEmpty() && OldFingerprint != Fingerprint) Previous.Reset();
		}
		for (const FString& Key : {TapDBConstants::PREF_PERSISTENT_ID, TapDBConstants::PREF_INSTALLATION_ID})
		{
			FString Value;
			if (Previous) Previous->TryGetStringField(Key, Value);
			if (Value.IsEmpty()) Value = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
			Identity->SetStringField(Key, Value);
		}
		if (!WriteObject(IdentityPath, Identity.ToSharedRef())) return false;
	}
	FString Persistent, Installation;
	if (!Identity->TryGetStringField(TapDBConstants::PREF_PERSISTENT_ID, Persistent) || Persistent.IsEmpty() ||
		!Identity->TryGetStringField(TapDBConstants::PREF_INSTALLATION_ID, Installation) || Installation.IsEmpty()) return false;
	FString PrivateDirectory;
	if (!bPrimary)
	{
		const FString Runs = FPaths::Combine(NewRoot, TEXT("runs"));
		if (!IFileManager::Get().MakeDirectory(*Runs, true) || !Safe(Runs, true)) return false;
		PrivateDirectory = FPaths::Combine(Runs, FGuid::NewGuid().ToString(EGuidFormats::Digits));
		auto Owner = MakeShared<FJsonObject>();
		if (!NewOwner(NewScope, Owner) || !WriteObject(FPaths::Combine(PrivateDirectory, TEXT("owner.json")), Owner)) return false;
	}
	Root = NewRoot; Scope = NewScope; ClientID = Client;
	PrefsPath = bPrimary ? LegacyPrefs : FPaths::Combine(PrivateDirectory, TEXT("prefs.json"));
	EventsPath = bPrimary ? LegacyEvents : FPaths::Combine(PrivateDirectory, TEXT("events.json"));
	PersistentId = Persistent; InstallationId = Installation;
	bIsolated = !bPrimary; bPrepared = true;
	return true;
}

bool FTapEventStorage::ExpiredOwner(const FString& Directory) const
{
	TSharedPtr<FJsonObject> Owner;
	FString OwnerScope, Started;
	double Pid = 0, Version = 0;
	if (!ReadObject(FPaths::Combine(Directory, TEXT("owner.json")), Owner) ||
		!Owner->TryGetNumberField(TEXT("version"), Version) || Version != 1 ||
		!Owner->TryGetStringField(TEXT("scope"), OwnerScope) || OwnerScope != Scope ||
		!Owner->TryGetNumberField(TEXT("pid"), Pid) || Pid < 1 || Pid > MAX_uint32 || Pid != FMath::FloorToDouble(Pid) ||
		!Owner->TryGetStringField(TEXT("started"), Started) || Started.IsEmpty()) return false;
	FString Current;
	bool bExited;
	return TapProcessIdentity::Query(static_cast<uint32>(Pid), Current, bExited) && (bExited || Current != Started);
}

bool FTapEventStorage::KnownContents(const FString& Directory) const
{
	if (!Safe(Directory, true)) return false;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*")), true, true);
	for (const FString& Name : Files)
	{
		if (Name != TEXT("owner.json") && Name != TEXT("prefs.json") && Name != TEXT("events.json") &&
			Name != TEXT("prefs.json.lock") && Name != TEXT("events.json.lock") && !CacheTemporary(Name)) return false;
		if (!Safe(FPaths::Combine(Directory, Name), false)) return false;
	}
	return true;
}

bool FTapEventStorage::ReadHistory(const FString& Directory, FEvents& Events) const
{
	const FString QueuePath = FPaths::Combine(Directory, TEXT("events.json"));
	if (IFileManager::Get().FileExists(*QueuePath))
	{
		TSharedPtr<FJsonObject> Queue;
		const TArray<TSharedPtr<FJsonValue>>* Array;
		if (!ReadObject(QueuePath, Queue) || !Queue->TryGetArrayField(TEXT("data"), Array)) return false;
		for (const auto& Item : *Array)
		{
			if (!Item || Item->Type != EJson::Object) return false;
			Events.Add(Item->AsObject());
		}
	}
	const FString StatePath = FPaths::Combine(Directory, TEXT("prefs.json"));
	if (IFileManager::Get().FileExists(*StatePath))
	{
		TSharedPtr<FJsonObject> State;
		if (!ReadObject(StatePath, State)) return false;
		for (const auto& Pair : State->Values)
			if (TapJson::KeyString(Pair.Key).StartsWith(TapDBConstants::PREF_PLAYED_DURATION)) return false;
		if (State->HasField(SnapshotKey))
		{
			FString Json, Name, User;
			TSharedPtr<FJsonObject> Snapshot;
			const TSharedPtr<FJsonObject>* Properties;
			double Duration = 0;
			if (!State->TryGetStringField(SnapshotKey, Json) ||
				!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Snapshot) || !Snapshot ||
				!Snapshot->TryGetStringField(TEXT("name"), Name) || Name != TapDBConstants::EVENT_PLAY_GAME ||
				!Snapshot->TryGetStringField(TEXT("user_id"), User) || User.IsEmpty() ||
				!Snapshot->TryGetObjectField(TEXT("properties"), Properties) || !Properties->IsValid() ||
				!(*Properties)->TryGetNumberField(TapDBConstants::PROP_DURATION, Duration) || !FMath::IsFinite(Duration) ||
				Duration < 0 || Duration != FMath::FloorToDouble(Duration)) return false;
			if (Duration > 0) Events.Add(Snapshot);
		}
	}
	for (const auto& Event : Events)
	{
		FString Client;
		if (!Event->TryGetStringField(TEXT("client_id"), Client) || Client != ClientID) return false;
	}
	return true;
}

void FTapEventStorage::Recover(FUpload Upload, TFunction<void()> Finished)
{
	if (!bPrepared) { if (Finished) Finished(); return; }
	auto Lease = FTapSharedCacheFile::TryAcquire(FPaths::Combine(Root, TEXT("recovery")));
	if (!Lease) { if (Finished) Finished(); return; }
	// Capture a value copy, never the SDK singleton's lifetime, across asynchronous HTTP.
	struct FRecovery : TSharedFromThis<FRecovery>
	{
		FTapEventStorage Storage;
		TUniquePtr<FTapSharedCacheFile::FLease> Lease;
		FUpload Upload;
		TFunction<void()> Finished;
		TArray<FString> Paths;
		int32 Index = 0;
		void Next()
		{
			while (Index < Paths.Num())
			{
				const FString Source = Paths[Index++];
				if (!Storage.ExpiredOwner(Source) || !Storage.KnownContents(Source)) continue;
				const FString Claims = FPaths::Combine(Storage.Root, TEXT("recover"));
				if (!IFileManager::Get().MakeDirectory(*Claims, true) || !Safe(Claims, true)) continue;
				const FString Destination = FPaths::Combine(Claims, FGuid::NewGuid().ToString(EGuidFormats::Digits));
				std::error_code Error;
				std::filesystem::rename(Native(Source), Native(Destination), Error);
				if (Error) continue;
				FEvents Events;
				if (!Storage.ReadHistory(Destination, Events)) continue;
				auto Self = AsShared();
				auto Complete = [Self, Destination](bool bSuccess)
				{
					// Both scanning and cleanup stay off the game/HTTP completion thread.
					Async(EAsyncExecution::ThreadPool, [Self, Destination, bSuccess]
					{
						if (bSuccess && Self->Storage.KnownContents(Destination))
						{
							TArray<FString> Files;
							IFileManager::Get().FindFiles(Files, *FPaths::Combine(Destination, TEXT("*")), true, false);
							// Leave owner metadata until the data files are gone, so partial cleanup remains recoverable.
							bool bClean = true;
							for (const FString& Name : Files)
								if (Name != TEXT("owner.json")) bClean &= IFileManager::Get().Delete(*FPaths::Combine(Destination, Name), false, true);
							if (bClean)
							{
								IFileManager::Get().Delete(*FPaths::Combine(Destination, TEXT("owner.json")), false, true);
								IFileManager::Get().DeleteDirectory(*Destination, false, false);
							}
						}
						Self->Next();
					});
				};
				if (Events.IsEmpty()) { Complete(true); return; }
				Upload(Events, MoveTemp(Complete));
				return;
			}
			Lease.Reset();
			if (Finished) Finished();
		}
	};
	auto Recovery = MakeShared<FRecovery>();
	Recovery->Storage = *this;
	Recovery->Lease = MoveTemp(Lease);
	Recovery->Upload = MoveTemp(Upload);
	Recovery->Finished = MoveTemp(Finished);
	Async(EAsyncExecution::ThreadPool, [Recovery]
	{
		for (const TCHAR* Name : {TEXT("recover"), TEXT("runs")})
		{
			const FString Directory = FPaths::Combine(Recovery->Storage.Root, Name);
			if (!Safe(Directory, true)) continue;
			TArray<FString> Children;
			IFileManager::Get().FindFiles(Children, *FPaths::Combine(Directory, TEXT("*")), false, true);
			for (const FString& Child : Children) Recovery->Paths.Add(FPaths::Combine(Directory, Child));
		}
		Recovery->Next();
	});
}

#else
FTapEventStorage& FTapEventStorage::Get() { static FTapEventStorage Storage; return Storage; }
bool FTapEventStorage::Prepare(const FString&, ETapRegion) { return true; }
bool FTapEventStorage::Initialize(const FString&, const FString&, ETapRegion, bool) { return false; }
void FTapEventStorage::Recover(FUpload, TFunction<void()> Finished) { if (Finished) Finished(); }
#endif

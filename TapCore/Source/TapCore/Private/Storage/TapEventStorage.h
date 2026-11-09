// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "TapBasicTypes.h"

/** Windows Player statistics storage. No public SDK option or slot selection required. */
class FTapEventStorage
{
public:
	using FEvents = TArray<TSharedPtr<FJsonObject>>;
	using FUpload = TFunction<void(const FEvents&, TFunction<void(bool)>)>;
	static FTapEventStorage& Get();
	bool Prepare(const FString& Client, ETapRegion Region);
	// Also used by real subprocess tests on Mac; production activation remains Windows Player only.
	bool Initialize(const FString& SavedRoot, const FString& Client, ETapRegion Region, bool bPrimary);
	bool IsIsolated() const { return bIsolated; }
	const FString& GetPrefsPath() const { return PrefsPath; }
	const FString& GetEventsPath() const { return EventsPath; }
	const FString& GetPersistentId() const { return PersistentId; }
	const FString& GetInstallationId() const { return InstallationId; }
	void Recover(FUpload Upload, TFunction<void()> Finished = nullptr);
	static constexpr const TCHAR* SnapshotKey = TEXT("tapdb_duration_snapshot");
private:
	bool ExpiredOwner(const FString& Directory) const;
	bool ReadHistory(const FString& Directory, FEvents& Events) const;
	bool KnownContents(const FString& Directory) const;
	FString Root, Scope, ClientID, PrefsPath, EventsPath, PersistentId, InstallationId;
	bool bIsolated = false;
	bool bPrepared = false;
	bool bWarnedUnavailable = false;
};

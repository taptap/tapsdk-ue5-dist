// Copyright TapTap. All Rights Reserved.

#include "CompliancePersistence.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "ComplianceState.h"
#include "TapComplianceModule.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Internal/Storage/TapSharedCacheFile.h"
#include "Containers/Ticker.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// =============================================================================
// 按用户分文件；公共配置独立保存。每次操作加载当前文件，避免复用其他用户的快照。
// =============================================================================

namespace
{
	struct FPendingCache
	{
		TSharedPtr<FTapMachineBoundPrefs> Prefs;
		TSet<FString> ClearMarkers;
	};
	// Keep failed writes alive beyond a single API call; serialize module operations with CacheLock.
	TMap<FString, FPendingCache> PendingCaches;
	FCriticalSection CacheLock;
	TSet<FString> InvalidMarkers;
	FTSTicker::FDelegateHandle RetryTicker;
	struct FRetryCleanup
	{
		~FRetryCleanup() { FTSTicker::GetCoreTicker().RemoveTicker(RetryTicker); }
	} RetryCleanup;

	FString MarkerPath(const FTapMachineBoundPrefs& Prefs, const FString& Key)
	{
		return Prefs.GetFilePath() + TEXT(".invalid.") + FMD5::HashAnsiString(*Key);
	}

	void FlushPendingCaches()
	{
		FScopeLock Guard(&CacheLock);
		for (auto It = PendingCaches.CreateIterator(); It; ++It)
		{
			auto& Pending = It.Value();
			Pending.Prefs->Save();
			if (!Pending.Prefs->IsPersisted()) continue;
			for (auto Marker = Pending.ClearMarkers.CreateIterator(); Marker; ++Marker)
			{
				if (IFileManager::Get().Delete(**Marker, false, true))
				{
					InvalidMarkers.Remove(*Marker);
					Marker.RemoveCurrent();
				}
			}
			if (Pending.ClearMarkers.IsEmpty()) It.RemoveCurrent();
		}
	}

	void RetainPending(const TSharedPtr<FTapMachineBoundPrefs>& Prefs)
	{
		PendingCaches.FindOrAdd(Prefs->GetFilePath()).Prefs = Prefs;
		if (!RetryTicker.IsValid())
		{
			RetryTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FScopeLock Guard(&CacheLock);
				FlushPendingCaches();
				if (!PendingCaches.IsEmpty()) return true;
				RetryTicker.Reset();
				return false;
			}), 1.0f);
		}
	}

	bool IsInvalid(const FTapMachineBoundPrefs& Prefs, const FString& Key)
	{
		const FString Marker = MarkerPath(Prefs, Key);
		return InvalidMarkers.Contains(Marker) || IFileManager::Get().FileExists(*Marker);
	}

	bool SaveValue(const TSharedPtr<FTapMachineBoundPrefs>& Prefs, const FString& Key, const FString& Value)
	{
		Prefs->Set(Key, Value);
		RetainPending(Prefs);
		PendingCaches[Prefs->GetFilePath()].ClearMarkers.Add(MarkerPath(*Prefs, Key));
		FlushPendingCaches();
		return Prefs->IsPersisted() && !IsInvalid(*Prefs, Key);
	}

	bool DeleteValue(const TSharedPtr<FTapMachineBoundPrefs>& Prefs, const FString& Key)
	{
		const FString Marker = MarkerPath(*Prefs, Key);
		InvalidMarkers.Add(Marker);
		// Independent of the busy data-file lease; survives a crash before deferred cleanup.
		const bool bInvalidated = FTapSharedCacheFile::WriteUnderLock(Marker, TEXT("invalid"));
		Prefs->Remove(Key); // Records deletion even if initial loading is deferred.
		RetainPending(Prefs);
		PendingCaches[Prefs->GetFilePath()].ClearMarkers.Remove(Marker);
		FlushPendingCaches();
		return bInvalidated || Prefs->IsPersisted();
	}
}

TSharedPtr<FTapMachineBoundPrefs> FCompliancePersistence::LoadPrefs(
	const FString& FileName, const TArray<FString>& LegacyKeys)
{
	const FString& ClientId = FComplianceState::Get().ClientId;
	const FString Sub = ClientId.IsEmpty() ? TEXT("default") : ClientId;
	const FString Directory = FPaths::Combine(TEXT("TapCompliance"), Sub);
	const FString RelativePath = FPaths::Combine(Directory, FileName);
	FlushPendingCaches();
	if (const auto* Pending = PendingCaches.Find(FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath)))
		return Pending->Prefs;
	const bool bAlreadyExists = IFileManager::Get().FileExists(
		*FPaths::Combine(FPaths::ProjectSavedDir(), RelativePath));
	TSharedPtr<FJsonObject> Legacy;
	const FString LegacyPath = FPaths::Combine(FPaths::ProjectSavedDir(), Directory, TEXT("cache.json"));
	if (!bAlreadyExists && !LegacyKeys.IsEmpty() && IFileManager::Get().FileExists(*LegacyPath))
	{
		// Do not initialize/rebuild a legacy Prefs or create a migration marker before it can be read.
		auto Lease = FTapSharedCacheFile::TryAcquire(LegacyPath);
		FString Json;
		if (!Lease || !FFileHelper::LoadFileToString(Json, *LegacyPath) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Legacy) || !Legacy)
			return nullptr;
	}
	auto Prefs = MakeShared<FTapMachineBoundPrefs>();
	Prefs->Init(RelativePath);

	// 已有新文件始终优先，包括已登出或机器指纹失效后清空的文件。
	if (!bAlreadyExists && Legacy)
	{
		FString OldFingerprint;
		const FString Fingerprint = FTapMachineBoundPrefs::ComputeFingerprint();
		if (!Fingerprint.IsEmpty() && Legacy->TryGetStringField(TEXT("tap_machine_fp"), OldFingerprint) && OldFingerprint == Fingerprint)
		{
			for (const FString& Key : LegacyKeys)
			{
				FString Value;
				if (Legacy->TryGetStringField(Key, Value) && !Value.IsEmpty())
				{
					Prefs->Set(Key, Value);
				}
			}
			RetainPending(Prefs);
			FlushPendingCaches();
		}
	}
	return Prefs;
}

TSharedPtr<FTapMachineBoundPrefs> FCompliancePersistence::LoadUserPrefs(const FString& UserId)
{
	return LoadPrefs(FPaths::Combine(TEXT("users"), HashUserId(UserId) + TEXT(".json")),
		{KeyForVerification(UserId), KeyForUserAntiConfig(UserId), KeyForFirstPassTip(UserId)});
}

TSharedPtr<FTapMachineBoundPrefs> FCompliancePersistence::LoadConfigPrefs()
{
	return LoadPrefs(TEXT("config.json"), {KeyForRealNameConfig()});
}

// =============================================================================
// Key / Hash 辅助
// =============================================================================

FString FCompliancePersistence::HashUserId(const FString& UserId)
{
	if (UserId.IsEmpty())
	{
		return TEXT("anonymous");
	}
	return FMD5::HashAnsiString(*UserId);
}

FString FCompliancePersistence::KeyForVerification(const FString& UserId)
{
	return FString::Printf(TEXT("verification:%s"), *HashUserId(UserId));
}

FString FCompliancePersistence::KeyForUserAntiConfig(const FString& UserId)
{
	return FString::Printf(TEXT("user_config:%s"), *HashUserId(UserId));
}

FString FCompliancePersistence::KeyForRealNameConfig()
{
	return TEXT("realname_config");
}

FString FCompliancePersistence::KeyForFirstPassTip(const FString& UserId)
{
	return FString::Printf(TEXT("first_pass_tip:%s"), *HashUserId(UserId));
}

// =============================================================================
// JSON 序列化辅助
// =============================================================================

FString FCompliancePersistence::SerializeJsonObject(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid())
	{
		return FString();
	}
	FString Out;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
	if (!FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer))
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[Persistence] JSON serialize failed"));
		return FString();
	}
	return Out;
}

TSharedPtr<FJsonObject> FCompliancePersistence::ParseJsonString(const FString& Json)
{
	if (Json.IsEmpty())
	{
		return nullptr;
	}
	TSharedPtr<FJsonObject> Obj;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
	{
		UE_LOG(LogTapCompliance, Warning, TEXT("[Persistence] JSON parse failed"));
		return nullptr;
	}
	return Obj;
}

// =============================================================================
// FComplianceVerification
// =============================================================================

bool FCompliancePersistence::LoadVerification(const FString& UserId, FComplianceVerification& OutResult)
{
	FScopeLock Guard(&CacheLock);
	auto Prefs = LoadUserPrefs(UserId);
	if (!Prefs) return false;
	const FString Raw = Prefs->GetString(KeyForVerification(UserId));
	if (IsInvalid(*Prefs, KeyForVerification(UserId))) return false;
	if (Raw.IsEmpty())
	{
		return false;
	}
	TSharedPtr<FJsonObject> Obj = ParseJsonString(Raw);
	if (!Obj.IsValid())
	{
		return false;
	}
	OutResult.FromJson(Obj);
	// 持久化数据里 user_id 缺失时, 至少回填当前 userId
	if (OutResult.UserId.IsEmpty())
	{
		OutResult.UserId = UserId;
	}
	return !OutResult.ComplianceToken.IsEmpty();
}

bool FCompliancePersistence::SaveVerification(const FString& UserId, const FComplianceVerification& Result)
{
	FScopeLock Guard(&CacheLock);
	FComplianceVerification ToSave = Result;
	if (ToSave.UserId.IsEmpty())
	{
		ToSave.UserId = UserId;
	}
	const FString Json = SerializeJsonObject(ToSave.ToJson());
	if (Json.IsEmpty())
	{
		return false;
	}
	auto Prefs = LoadUserPrefs(UserId);
	if (!Prefs) return false;
	return SaveValue(Prefs, KeyForVerification(UserId), Json);
}

bool FCompliancePersistence::DeleteVerification(const FString& UserId)
{
	FScopeLock Guard(&CacheLock);
	// Logout must not depend on reading/migrating a busy legacy cache.
	auto Prefs = LoadPrefs(FPaths::Combine(TEXT("users"), HashUserId(UserId) + TEXT(".json")), {});
	if (!Prefs) return false;
	return DeleteValue(Prefs, KeyForVerification(UserId));
}

// =============================================================================
// FUserAntiConfig
// =============================================================================

bool FCompliancePersistence::LoadUserAntiConfig(const FString& UserId, FUserAntiConfig& OutResult)
{
	FScopeLock Guard(&CacheLock);
	auto Prefs = LoadUserPrefs(UserId);
	if (!Prefs) return false;
	const FString Raw = Prefs->GetString(KeyForUserAntiConfig(UserId));
	if (IsInvalid(*Prefs, KeyForUserAntiConfig(UserId))) return false;
	if (Raw.IsEmpty())
	{
		return false;
	}
	TSharedPtr<FJsonObject> Obj = ParseJsonString(Raw);
	if (!Obj.IsValid())
	{
		return false;
	}
	OutResult.FromJson(Obj);
	return true;
}

bool FCompliancePersistence::SaveUserAntiConfig(const FString& UserId, const FUserAntiConfig& Result)
{
	FScopeLock Guard(&CacheLock);
	const FString Json = SerializeJsonObject(Result.ToJson());
	if (Json.IsEmpty())
	{
		return false;
	}
	auto Prefs = LoadUserPrefs(UserId);
	if (!Prefs) return false;
	return SaveValue(Prefs, KeyForUserAntiConfig(UserId), Json);
}

bool FCompliancePersistence::DeleteUserAntiConfig(const FString& UserId)
{
	FScopeLock Guard(&CacheLock);
	auto Prefs = LoadPrefs(FPaths::Combine(TEXT("users"), HashUserId(UserId) + TEXT(".json")), {});
	if (!Prefs) return false;
	return DeleteValue(Prefs, KeyForUserAntiConfig(UserId));
}

// =============================================================================
// FRealNameConfigResult
// =============================================================================

bool FCompliancePersistence::LoadRealNameConfig(FRealNameConfigResult& OutResult)
{
	FScopeLock Guard(&CacheLock);
	auto Prefs = LoadConfigPrefs();
	if (!Prefs) return false;
	const FString Raw = Prefs->GetString(KeyForRealNameConfig());
	if (Raw.IsEmpty())
	{
		return false;
	}
	TSharedPtr<FJsonObject> Obj = ParseJsonString(Raw);
	if (!Obj.IsValid())
	{
		return false;
	}
	OutResult.FromJson(Obj);
	return true;
}

bool FCompliancePersistence::SaveRealNameConfig(const FRealNameConfigResult& Result)
{
	FScopeLock Guard(&CacheLock);
	const FString Json = SerializeJsonObject(Result.ToJson());
	if (Json.IsEmpty())
	{
		return false;
	}
	auto Prefs = LoadConfigPrefs();
	if (!Prefs) return false;
	return SaveValue(Prefs, KeyForRealNameConfig(), Json);
}

// =============================================================================
// 一次性 UI 标记
// =============================================================================

bool FCompliancePersistence::HasShownFirstPassTip(const FString& UserId)
{
	FScopeLock Guard(&CacheLock);
	auto Prefs = LoadUserPrefs(UserId);
	if (!Prefs) return false;
	return !Prefs->GetString(KeyForFirstPassTip(UserId)).IsEmpty();
}

void FCompliancePersistence::MarkFirstPassTipShown(const FString& UserId)
{
	FScopeLock Guard(&CacheLock);
	auto Prefs = LoadUserPrefs(UserId);
	if (!Prefs) return;
	SaveValue(Prefs, KeyForFirstPassTip(UserId), TEXT("1"));
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

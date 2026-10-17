// Copyright TapTap. All Rights Reserved.

#include "CompliancePersistence.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "ComplianceState.h"
#include "TapComplianceModule.h"
#include "Internal/Storage/TapMachineBoundPrefs.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// =============================================================================
// 单例 FTapMachineBoundPrefs
// =============================================================================

FTapMachineBoundPrefs& FCompliancePersistence::GetPrefs()
{
	static FTapMachineBoundPrefs Prefs;
	static bool bInited = false;
	if (!bInited)
	{
		const FString& ClientId = FComplianceState::Get().ClientId;
		const FString Sub = ClientId.IsEmpty() ? TEXT("default") : ClientId;
		// FTapMachineBoundPrefs.Init 接受相对 ProjectSavedDir 的路径
		const FString RelativePath = FPaths::Combine(TEXT("TapCompliance"), Sub, TEXT("cache.json"));
		Prefs.Init(RelativePath);
		bInited = true;

		if (Prefs.WasInvalidated())
		{
			UE_LOG(LogTapCompliance, Warning,
				TEXT("[Persistence] Machine fingerprint mismatch — TapCompliance cache wiped, will refetch from server"));
		}
	}
	return Prefs;
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
	const FString Raw = GetPrefs().GetString(KeyForVerification(UserId));
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
	GetPrefs().Set(KeyForVerification(UserId), Json);
	GetPrefs().Save();
	return true;
}

bool FCompliancePersistence::DeleteVerification(const FString& UserId)
{
	const bool bRemoved = GetPrefs().Remove(KeyForVerification(UserId));
	if (bRemoved)
	{
		GetPrefs().Save();
	}
	return bRemoved;
}

// =============================================================================
// FUserAntiConfig
// =============================================================================

bool FCompliancePersistence::LoadUserAntiConfig(const FString& UserId, FUserAntiConfig& OutResult)
{
	const FString Raw = GetPrefs().GetString(KeyForUserAntiConfig(UserId));
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
	const FString Json = SerializeJsonObject(Result.ToJson());
	if (Json.IsEmpty())
	{
		return false;
	}
	GetPrefs().Set(KeyForUserAntiConfig(UserId), Json);
	GetPrefs().Save();
	return true;
}

bool FCompliancePersistence::DeleteUserAntiConfig(const FString& UserId)
{
	const bool bRemoved = GetPrefs().Remove(KeyForUserAntiConfig(UserId));
	if (bRemoved)
	{
		GetPrefs().Save();
	}
	return bRemoved;
}

// =============================================================================
// FRealNameConfigResult
// =============================================================================

bool FCompliancePersistence::LoadRealNameConfig(FRealNameConfigResult& OutResult)
{
	const FString Raw = GetPrefs().GetString(KeyForRealNameConfig());
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
	const FString Json = SerializeJsonObject(Result.ToJson());
	if (Json.IsEmpty())
	{
		return false;
	}
	GetPrefs().Set(KeyForRealNameConfig(), Json);
	GetPrefs().Save();
	return true;
}

// =============================================================================
// 一次性 UI 标记
// =============================================================================

bool FCompliancePersistence::HasShownFirstPassTip(const FString& UserId)
{
	return !GetPrefs().GetString(KeyForFirstPassTip(UserId)).IsEmpty();
}

void FCompliancePersistence::MarkFirstPassTipShown(const FString& UserId)
{
	GetPrefs().Set(KeyForFirstPassTip(UserId), TEXT("1"));
	GetPrefs().Save();
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

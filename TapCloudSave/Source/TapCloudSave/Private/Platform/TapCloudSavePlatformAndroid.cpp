// Copyright TapTap. All Rights Reserved.

#include "Platform/TapCloudSavePlatformAndroid.h"

#if PLATFORM_ANDROID

#include "TapCloudSaveModule.h"
#include "Mobile/Android/TapCloudSaveAndroidBridge.h"

#include "Async/Async.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	void RunOnGameThread(TFunction<void()> Fn)
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(Fn));
	}

	// Mirror the TapCloudSaveUERouter envelope (EngineBridgeResult.toJson()):
	//   { "code": 0|-1, "message": "success|fail", "content": "<inner json | base64 | int>" }
	struct FBridgeEnvelope
	{
		int32   Code = -1;
		FString Content; // inner JSON string OR base64
	};

	bool ParseEnvelope(const FString& Json, FBridgeEnvelope& Out)
	{
		TSharedPtr<FJsonObject> Root;
		auto Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		const bool bHasEnvCode    = Root->HasField(TEXT("code"));
		const bool bHasEnvContent = Root->HasField(TEXT("content"));
		const bool bHasErrInner   = Root->HasField(TEXT("errorCode")) || Root->HasField(TEXT("errorMessage"));
		// Defensive: if the bridge ever fires raw inner error JSON instead of
		// the standard envelope, treat the whole payload as inner error
		// content (mirrors PlatformIOS sync-validation-error handling).
		if (!bHasEnvCode && !bHasEnvContent && bHasErrInner)
		{
			Out.Code = -1;
			Out.Content = Json;
			return true;
		}

		Root->TryGetNumberField(TEXT("code"), Out.Code);
		Root->TryGetStringField(TEXT("content"), Out.Content);
		return true;
	}

	// Error inner content (Router buildErrorJson):
	//   { "errorCode": N, "errorMessage": "..." }
	void ParseErrorInner(const FString& InnerJson, int32& OutCode, FString& OutMessage)
	{
		OutCode = -1;
		OutMessage.Reset();

		TSharedPtr<FJsonObject> Root;
		auto Reader = TJsonReaderFactory<>::Create(InnerJson);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			OutMessage = InnerJson;
			return;
		}
		int32 Tmp = -1;
		if (Root->TryGetNumberField(TEXT("errorCode"), Tmp)) { OutCode = Tmp; }
		Root->TryGetStringField(TEXT("errorMessage"), OutMessage);
		if (OutMessage.IsEmpty()) { OutMessage = TEXT("unknown"); }
	}

	// ArchiveData inner JSON (snake_case from kotlinx.serialization @SerialName):
	//   uuid / file_id / name / summary / extra / playtime / save_size /
	//   cover_size / created_time / modified_time
	bool ParseArchive(const TSharedPtr<FJsonObject>& Root, FTapArchiveData& Out)
	{
		if (!Root.IsValid()) { return false; }
		Root->TryGetStringField(TEXT("uuid"),    Out.Uuid);
		Root->TryGetStringField(TEXT("file_id"), Out.FileId);
		Root->TryGetStringField(TEXT("name"),    Out.Name);
		Root->TryGetStringField(TEXT("summary"), Out.Summary);
		Root->TryGetStringField(TEXT("extra"),   Out.Extra);
		int32 PT = 0;
		if (Root->TryGetNumberField(TEXT("playtime"), PT)) { Out.Playtime = PT; }
		Root->TryGetNumberField(TEXT("save_size"),     Out.ArchiveFileSize);
		Root->TryGetNumberField(TEXT("cover_size"),    Out.ArchiveCoverSize);
		Root->TryGetNumberField(TEXT("created_time"),  Out.CreatedTime);
		Root->TryGetNumberField(TEXT("modified_time"), Out.ModifiedTime);
		return true;
	}

	bool ParseArchiveJsonString(const FString& Json, FTapArchiveData& Out)
	{
		TSharedPtr<FJsonObject> Root;
		auto Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		return ParseArchive(Root, Out);
	}

	FString SerializeMeta(const FTapArchiveMetadata& M)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("name"),     M.Name);
		J->SetStringField(TEXT("summary"),  M.Summary);
		J->SetStringField(TEXT("extra"),    M.Extra);
		J->SetNumberField(TEXT("playtime"), M.Playtime);
		FString Out;
		auto Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(J, Writer);
		return Out;
	}

	void FireError(const FTapOnArchiveError& OnError, int32 Code, FString Msg)
	{
		RunOnGameThread([OnError, Code, Msg = MoveTemp(Msg)]
		{
			OnError.ExecuteIfBound(Code, Msg);
		});
	}

	bool HandleEnvelopeOrFireError(const FString& Tag, bool bHasResult, const FString& RawJson,
	                               const FTapOnArchiveError& OnError,
	                               FBridgeEnvelope& OutEnvelope)
	{
		if (!bHasResult)
		{
			FireError(OnError, -1, FString::Printf(TEXT("%s: native bridge returned nil"), *Tag));
			return false;
		}
		if (!ParseEnvelope(RawJson, OutEnvelope))
		{
			FireError(OnError, -1, FString::Printf(TEXT("%s: envelope parse failed: %s"), *Tag, *RawJson));
			return false;
		}
		if (OutEnvelope.Code != 0)
		{
			int32 InnerCode = -1; FString InnerMsg;
			ParseErrorInner(OutEnvelope.Content, InnerCode, InnerMsg);
			FireError(OnError, InnerCode, FString::Printf(TEXT("%s failed: %s"), *Tag, *InnerMsg));
			return false;
		}
		return true;
	}
}

FTapCloudSavePlatformAndroid::FTapCloudSavePlatformAndroid() = default;

FTapCloudSavePlatformAndroid::~FTapCloudSavePlatformAndroid()
{
	FTapTapSDK::RemoveInitCallback(this);
	// Drop our state forwarder so any in-flight native callback no-ops.
	if (bNativeStateCallbackRegistered)
	{
		TapCloudSaveAndroidBridge::UnregisterStateCallback();
	}
}

bool FTapCloudSavePlatformAndroid::IsInitialized() const
{
	return bInitialized;
}

void FTapCloudSavePlatformAndroid::Initialize()
{
	if (bNativeStateCallbackRegistered) { return; }
	if (!bInitialized)
	{
		TapCloudSaveAndroidBridge::Init();
		bInitialized = true;
	}

	// InitTask runs before Android's asynchronous initialization is complete.
	// AddInitCallback deduplicates listeners and replays success for late subscribers.
	FTapTapSDK::AddInitCallback(this);
	UE_LOG(LogTapCloudSave, Log, TEXT("[CloudSave-Android] Bridge ready; waiting for SDK initialization success"));
}

void FTapCloudSavePlatformAndroid::OnInitSuccess()
{
	if (bNativeStateCallbackRegistered || !FTapTapSDK::IsInitialized()) { return; }

	TWeakPtr<FTapCloudSavePlatformAndroid> WeakSelf = AsShared();
	bNativeStateCallbackRegistered = TapCloudSaveAndroidBridge::RegisterStateCallback([WeakSelf](int32 RawCode)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }
		Self->HandleRawState(RawCode);
	});

	if (bNativeStateCallbackRegistered)
	{
		FTapTapSDK::RemoveInitCallback(this);
		UE_LOG(LogTapCloudSave, Log, TEXT("[CloudSave-Android] Native state callback registered after SDK initialization success"));
	}
}

void FTapCloudSavePlatformAndroid::OnInitFail(int32 ErrorCode, const FString& ErrorMessage)
{
	// Keep the listener so a later successful initialization retry registers the callback.
	UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave-Android] Native callback registration deferred after SDK initialization failure: %d %s"),
		ErrorCode, *ErrorMessage);
}

void FTapCloudSavePlatformAndroid::HandleRawState(int32 RawCode)
{
	ETapCloudSaveState State = ETapCloudSaveState::Unknown;
	switch (RawCode)
	{
		case TapCloudSaveResultCode::NEED_LOGIN: State = ETapCloudSaveState::NeedLogin; break;
		case TapCloudSaveResultCode::INIT_FAIL:  State = ETapCloudSaveState::InitFail;  break;
		default: break;
	}
	NotifyState(State, RawCode);
}

void FTapCloudSavePlatformAndroid::NotifyState(ETapCloudSaveState State, int32 RawCode)
{
	TArray<FTapOnCloudSaveState> Snapshot;
	{
		FScopeLock Lock(&StateLock);
		Snapshot = StateCallbacks;
	}
	RunOnGameThread([Snapshot, State, RawCode]
	{
		for (const auto& Cb : Snapshot)
		{
			Cb.ExecuteIfBound(State, RawCode);
		}
	});
}

void FTapCloudSavePlatformAndroid::RegisterStateCallback(const FTapOnCloudSaveState& OnState)
{
	FScopeLock Lock(&StateLock);
	for (const auto& Existing : StateCallbacks)
	{
		if (Existing.GetHandle() == OnState.GetHandle())
		{
			UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave-Android] state callback already registered"));
			return;
		}
	}
	StateCallbacks.Add(OnState);
}

void FTapCloudSavePlatformAndroid::UnregisterStateCallback(const FTapOnCloudSaveState& OnState)
{
	FScopeLock Lock(&StateLock);
	const int32 Removed = StateCallbacks.RemoveAll([&](const FTapOnCloudSaveState& E)
	{
		return E.GetHandle() == OnState.GetHandle();
	});
	if (Removed == 0)
	{
		UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave-Android] state callback not found"));
	}
}

// ---------------------------------------------------------------------------
// Business APIs
// ---------------------------------------------------------------------------

void FTapCloudSavePlatformAndroid::CreateArchive(
	const FTapArchiveMetadata& Metadata,
	const FString& ArchiveFilePath,
	const FString& CoverFilePath,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError&  OnError)
{
	const FString MetaJson = SerializeMeta(Metadata);
	TWeakPtr<FTapCloudSavePlatformAndroid> WeakSelf = AsShared();

	// UE5 虚拟相对路径（../../../TapSDKDemo/Saved/...）无法被 Android Java 侧直接打开；
	// 转换为 Android 实际绝对路径后再传给 JNI Bridge。
	const FString AbsArchivePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ArchiveFilePath);
	const FString AbsCoverPath   = CoverFilePath.IsEmpty()
		? FString()
		: IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CoverFilePath);

	TapCloudSaveAndroidBridge::CreateArchive(MetaJson, AbsArchivePath, AbsCoverPath,
		[WeakSelf, OnSuccess, OnError, Metadata](bool bHasResult, const FString& Raw)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }

		FBridgeEnvelope Env;
		if (!HandleEnvelopeOrFireError(TEXT("createArchive"), bHasResult, Raw, OnError, Env)) { return; }

		FTapArchiveData Data;
		if (!ParseArchiveJsonString(Env.Content, Data))
		{
			FireError(OnError, -1, FString::Printf(TEXT("createArchive: inner parse failed: %s"), *Env.Content));
			return;
		}
		if (Data.Name.IsEmpty())    { Data.Name    = Metadata.Name; }
		if (Data.Summary.IsEmpty()) { Data.Summary = Metadata.Summary; }
		if (Data.Extra.IsEmpty())   { Data.Extra   = Metadata.Extra; }
		if (Data.Playtime == 0)     { Data.Playtime = Metadata.Playtime; }

		RunOnGameThread([OnSuccess, Data]{ OnSuccess.ExecuteIfBound(Data); });
	});
}

void FTapCloudSavePlatformAndroid::UpdateArchive(
	const FString& Uuid,
	const FTapArchiveMetadata& Metadata,
	const FString& ArchiveFilePath,
	const FString& CoverFilePath,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError&  OnError)
{
	const FString MetaJson = SerializeMeta(Metadata);
	TWeakPtr<FTapCloudSavePlatformAndroid> WeakSelf = AsShared();

	const FString AbsArchivePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ArchiveFilePath);
	const FString AbsCoverPath   = CoverFilePath.IsEmpty()
		? FString()
		: IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*CoverFilePath);

	TapCloudSaveAndroidBridge::UpdateArchive(Uuid, MetaJson, AbsArchivePath, AbsCoverPath,
		[WeakSelf, Uuid, OnSuccess, OnError, Metadata](bool bHasResult, const FString& Raw)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }

		FBridgeEnvelope Env;
		if (!HandleEnvelopeOrFireError(TEXT("updateArchive"), bHasResult, Raw, OnError, Env)) { return; }

		FTapArchiveData Data;
		if (!ParseArchiveJsonString(Env.Content, Data))
		{
			FireError(OnError, -1, FString::Printf(TEXT("updateArchive: inner parse failed: %s"), *Env.Content));
			return;
		}
		if (Data.Uuid.IsEmpty())    { Data.Uuid    = Uuid; }
		if (Data.Name.IsEmpty())    { Data.Name    = Metadata.Name; }
		if (Data.Summary.IsEmpty()) { Data.Summary = Metadata.Summary; }
		if (Data.Extra.IsEmpty())   { Data.Extra   = Metadata.Extra; }
		if (Data.Playtime == 0)     { Data.Playtime = Metadata.Playtime; }

		RunOnGameThread([OnSuccess, Data]{ OnSuccess.ExecuteIfBound(Data); });
	});
}

void FTapCloudSavePlatformAndroid::DeleteArchive(
	const FString& Uuid,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError&  OnError)
{
	TWeakPtr<FTapCloudSavePlatformAndroid> WeakSelf = AsShared();

	TapCloudSaveAndroidBridge::DeleteArchive(Uuid,
		[WeakSelf, Uuid, OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }

		FBridgeEnvelope Env;
		if (!HandleEnvelopeOrFireError(TEXT("deleteArchive"), bHasResult, Raw, OnError, Env)) { return; }

		FTapArchiveData Data;
		// onArchiveDeleted returns the archive payload; parse-failure is non-fatal —
		// fall back to the input Uuid so callers always get a populated record.
		ParseArchiveJsonString(Env.Content, Data);
		if (Data.Uuid.IsEmpty()) { Data.Uuid = Uuid; }

		RunOnGameThread([OnSuccess, Data]{ OnSuccess.ExecuteIfBound(Data); });
	});
}

void FTapCloudSavePlatformAndroid::GetArchiveList(
	const FTapOnArchiveListResult& OnSuccess,
	const FTapOnArchiveError&      OnError)
{
	TWeakPtr<FTapCloudSavePlatformAndroid> WeakSelf = AsShared();

	TapCloudSaveAndroidBridge::GetArchiveList(
		[WeakSelf, OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }

		FBridgeEnvelope Env;
		if (!HandleEnvelopeOrFireError(TEXT("getArchiveList"), bHasResult, Raw, OnError, Env)) { return; }

		// Inner is a JSON array of archive objects.
		TArray<FTapArchiveData> List;
		TArray<TSharedPtr<FJsonValue>> Items;
		auto Reader = TJsonReaderFactory<>::Create(Env.Content);
		if (!FJsonSerializer::Deserialize(Reader, Items))
		{
			FireError(OnError, -1, FString::Printf(TEXT("getArchiveList: array parse failed: %s"), *Env.Content));
			return;
		}
		for (const TSharedPtr<FJsonValue>& V : Items)
		{
			const TSharedPtr<FJsonObject> Obj = V.IsValid() ? V->AsObject() : nullptr;
			if (!Obj.IsValid()) { continue; }
			FTapArchiveData D;
			ParseArchive(Obj, D);
			List.Add(MoveTemp(D));
		}

		RunOnGameThread([OnSuccess, List]{ OnSuccess.ExecuteIfBound(List); });
	});
}

void FTapCloudSavePlatformAndroid::GetArchiveData(
	const FString& Uuid,
	const FString& FileId,
	const FTapOnArchiveBinaryResult& OnSuccess,
	const FTapOnArchiveError&        OnError)
{
	TWeakPtr<FTapCloudSavePlatformAndroid> WeakSelf = AsShared();

	TapCloudSaveAndroidBridge::GetArchiveData(Uuid, FileId,
		[WeakSelf, OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }

		FBridgeEnvelope Env;
		if (!HandleEnvelopeOrFireError(TEXT("getArchiveData"), bHasResult, Raw, OnError, Env)) { return; }

		TArray<uint8> Bytes;
		if (!FBase64::Decode(Env.Content, Bytes))
		{
			FireError(OnError, -1, TEXT("getArchiveData: base64 decode failed"));
			return;
		}

		RunOnGameThread([OnSuccess, Bytes]{ OnSuccess.ExecuteIfBound(Bytes); });
	});
}

void FTapCloudSavePlatformAndroid::GetArchiveCover(
	const FString& Uuid,
	const FString& FileId,
	const FTapOnArchiveBinaryResult& OnSuccess,
	const FTapOnArchiveError&        OnError)
{
	TWeakPtr<FTapCloudSavePlatformAndroid> WeakSelf = AsShared();

	TapCloudSaveAndroidBridge::GetArchiveCover(Uuid, FileId,
		[WeakSelf, OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }

		FBridgeEnvelope Env;
		if (!HandleEnvelopeOrFireError(TEXT("getArchiveCover"), bHasResult, Raw, OnError, Env)) { return; }

		TArray<uint8> Bytes;
		if (!FBase64::Decode(Env.Content, Bytes))
		{
			FireError(OnError, -1, TEXT("getArchiveCover: base64 decode failed"));
			return;
		}

		RunOnGameThread([OnSuccess, Bytes]{ OnSuccess.ExecuteIfBound(Bytes); });
	});
}

#endif // PLATFORM_ANDROID

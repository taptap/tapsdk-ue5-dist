// Copyright TapTap. All Rights Reserved.

#include "TapCloudSavePlatformIOS.h"

#include "TapCloudSaveModule.h"
#include "Mobile/iOS/TapCloudSaveIOSBridge.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
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

	// Mirror Unity Mobile bridge envelope (BridgeCloudSaveCallback.m):
	//   { "code": 0|-1, "content": "<jsonString | base64>", "message": "..." }
	struct FBridgeEnvelope
	{
		int32                   Code    = -1;
		FString                 Content;     // inner JSON string OR base64 OR plain int string
	};

	bool ParseEnvelope(const FString& Json, FBridgeEnvelope& Out)
	{
		TSharedPtr<FJsonObject> Root;
		auto Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}

		// Detect "synchronous validation error" shape from BridgeCloudSaveService:
		// the bridge sometimes fires the result callback with the raw inner error
		// payload {"errorCode":N,"errorMessage":"..."} instead of the standard
		// envelope. In that case, treat the whole JSON as the inner error content
		// so the downstream ParseErrorInner can extract the message.
		const bool bHasEnvCode    = Root->HasField(TEXT("code"));
		const bool bHasEnvContent = Root->HasField(TEXT("content"));
		const bool bHasErrInner   = Root->HasField(TEXT("errorCode")) || Root->HasField(TEXT("errorMessage"));
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

	// Error inner content (BridgeCloudSaveCallback.m onRequestError):
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

	// Archive inner content (BridgeCloudSaveCallback.m onArchiveCreated / Updated / Deleted):
	//   { "uuid":"…", "file_id":"…", "name":"…", "summary":"…", "extra":"…",
	//     "playtime":N, "save_size":N, "cover_size":N, "created_time":N, "modified_time":N }
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

	// Common dispatch: turn a bridge envelope into either OnError or a callable.
	// Returns true when the call should proceed to consume the inner content.
	// `Tag` is interpolated into error messages for diagnosability.
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

FTapCloudSavePlatformIOS::FTapCloudSavePlatformIOS() = default;

FTapCloudSavePlatformIOS::~FTapCloudSavePlatformIOS()
{
	// Drop our state forwarder so any in-flight native callback no-ops.
	TapCloudSaveIOSBridge::UnregisterStateCallback();
}

bool FTapCloudSavePlatformIOS::IsInitialized() const
{
	return bInitialized;
}

void FTapCloudSavePlatformIOS::Initialize()
{
	if (bInitialized) { return; }
	TapCloudSaveIOSBridge::Init();

	TWeakPtr<FTapCloudSavePlatformIOS> WeakSelf = AsShared();
	TapCloudSaveIOSBridge::RegisterStateCallback([WeakSelf](int32 RawCode)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }
		Self->HandleRawState(RawCode);
	});

	bInitialized = true;
	UE_LOG(LogTapCloudSave, Log, TEXT("[CloudSave-iOS] Initialize complete"));
}

void FTapCloudSavePlatformIOS::HandleRawState(int32 RawCode)
{
	// Map known raw codes to high-level state; unknown codes still fire
	// with State=Unknown so callers can log/inspect via the RawCode arg.
	ETapCloudSaveState State = ETapCloudSaveState::Unknown;
	switch (RawCode)
	{
		case TapCloudSaveResultCode::NEED_LOGIN: State = ETapCloudSaveState::NeedLogin; break;
		case TapCloudSaveResultCode::INIT_FAIL:  State = ETapCloudSaveState::InitFail;  break;
		default: break;
	}
	NotifyState(State, RawCode);
}

void FTapCloudSavePlatformIOS::NotifyState(ETapCloudSaveState State, int32 RawCode)
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

void FTapCloudSavePlatformIOS::RegisterStateCallback(const FTapOnCloudSaveState& OnState)
{
	FScopeLock Lock(&StateLock);
	for (const auto& Existing : StateCallbacks)
	{
		if (Existing.GetHandle() == OnState.GetHandle())
		{
			UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave-iOS] state callback already registered"));
			return;
		}
	}
	StateCallbacks.Add(OnState);
}

void FTapCloudSavePlatformIOS::UnregisterStateCallback(const FTapOnCloudSaveState& OnState)
{
	FScopeLock Lock(&StateLock);
	const int32 Removed = StateCallbacks.RemoveAll([&](const FTapOnCloudSaveState& E)
	{
		return E.GetHandle() == OnState.GetHandle();
	});
	if (Removed == 0)
	{
		UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave-iOS] state callback not found"));
	}
}

// ---------------------------------------------------------------------------
// Business APIs
// ---------------------------------------------------------------------------
//
// All callbacks come back on whatever queue the iOS SDK chose. We marshal
// every public OnSuccess/OnError to the GameThread via RunOnGameThread.

void FTapCloudSavePlatformIOS::CreateArchive(
	const FTapArchiveMetadata& Metadata,
	const FString& ArchiveFilePath,
	const FString& CoverFilePath,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError&  OnError)
{
	const FString MetaJson = SerializeMeta(Metadata);
	TWeakPtr<FTapCloudSavePlatformIOS> WeakSelf = AsShared();

	// UE5 虚拟相对路径（../../../TapSDKDemo/Saved/...）无法被 iOS 侧的
	// FileManager.fileExists(atPath:) 直接打开；转换为沙盒实际绝对路径后再传给 Bridge。
	// 用 ForRead：FIOSPlatformFile 会先 stat 只读的 Bundle 路径，未命中时自动回退到
	// 可写的 Documents 路径——正好覆盖"游戏运行时写入 Saved/"和"存档随包发布"两种来源。
	const FString AbsArchivePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ArchiveFilePath);
	const FString AbsCoverPath   = CoverFilePath.IsEmpty()
		? FString()
		: IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CoverFilePath);

	UE_LOG(LogTapCloudSave, Log,
		TEXT("[CloudSave-iOS] CreateArchive path mapping: archiveInput=%s archiveResolved=%s archiveExists=%d coverInput=%s coverResolved=%s coverExists=%d"),
		*ArchiveFilePath,
		*AbsArchivePath,
		IFileManager::Get().FileExists(*AbsArchivePath) ? 1 : 0,
		*CoverFilePath,
		*AbsCoverPath,
		AbsCoverPath.IsEmpty() ? 1 : (IFileManager::Get().FileExists(*AbsCoverPath) ? 1 : 0));

	TapCloudSaveIOSBridge::CreateArchive(MetaJson, AbsArchivePath, AbsCoverPath,
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
		// iOS bridge already populates name/summary/extra/playtime from the
		// SDK response, but echo from request to mirror Desktop behaviour
		// in case the SDK ever returns empty strings.
		if (Data.Name.IsEmpty())    { Data.Name    = Metadata.Name; }
		if (Data.Summary.IsEmpty()) { Data.Summary = Metadata.Summary; }
		if (Data.Extra.IsEmpty())   { Data.Extra   = Metadata.Extra; }
		if (Data.Playtime == 0)     { Data.Playtime = Metadata.Playtime; }

		RunOnGameThread([OnSuccess, Data]{ OnSuccess.ExecuteIfBound(Data); });
	});
}

void FTapCloudSavePlatformIOS::UpdateArchive(
	const FString& Uuid,
	const FTapArchiveMetadata& Metadata,
	const FString& ArchiveFilePath,
	const FString& CoverFilePath,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError&  OnError)
{
	const FString MetaJson = SerializeMeta(Metadata);
	TWeakPtr<FTapCloudSavePlatformIOS> WeakSelf = AsShared();

	// 同 CreateArchive：转换为沙盒实际绝对路径后再传给 Bridge。
	const FString AbsArchivePath = IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ArchiveFilePath);
	const FString AbsCoverPath   = CoverFilePath.IsEmpty()
		? FString()
		: IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*CoverFilePath);

	UE_LOG(LogTapCloudSave, Log,
		TEXT("[CloudSave-iOS] UpdateArchive path mapping: uuid=%s archiveInput=%s archiveResolved=%s archiveExists=%d coverInput=%s coverResolved=%s coverExists=%d"),
		*Uuid,
		*ArchiveFilePath,
		*AbsArchivePath,
		IFileManager::Get().FileExists(*AbsArchivePath) ? 1 : 0,
		*CoverFilePath,
		*AbsCoverPath,
		AbsCoverPath.IsEmpty() ? 1 : (IFileManager::Get().FileExists(*AbsCoverPath) ? 1 : 0));

	TapCloudSaveIOSBridge::UpdateArchive(Uuid, MetaJson, AbsArchivePath, AbsCoverPath,
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

void FTapCloudSavePlatformIOS::DeleteArchive(
	const FString& Uuid,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError&  OnError)
{
	TWeakPtr<FTapCloudSavePlatformIOS> WeakSelf = AsShared();

	TapCloudSaveIOSBridge::DeleteArchive(Uuid,
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

void FTapCloudSavePlatformIOS::GetArchiveList(
	const FTapOnArchiveListResult& OnSuccess,
	const FTapOnArchiveError&      OnError)
{
	TWeakPtr<FTapCloudSavePlatformIOS> WeakSelf = AsShared();

	TapCloudSaveIOSBridge::GetArchiveList(
		[WeakSelf, OnSuccess, OnError](bool bHasResult, const FString& Raw)
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) { return; }

		FBridgeEnvelope Env;
		if (!HandleEnvelopeOrFireError(TEXT("getArchiveList"), bHasResult, Raw, OnError, Env)) { return; }

		// Inner is a JSON array of archive objects (per onArchiveListResultWithArchives:).
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

void FTapCloudSavePlatformIOS::GetArchiveData(
	const FString& Uuid,
	const FString& FileId,
	const FTapOnArchiveBinaryResult& OnSuccess,
	const FTapOnArchiveError&        OnError)
{
	TWeakPtr<FTapCloudSavePlatformIOS> WeakSelf = AsShared();

	TapCloudSaveIOSBridge::GetArchiveData(Uuid, FileId,
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

void FTapCloudSavePlatformIOS::GetArchiveCover(
	const FString& Uuid,
	const FString& FileId,
	const FTapOnArchiveBinaryResult& OnSuccess,
	const FTapOnArchiveError&        OnError)
{
	TWeakPtr<FTapCloudSavePlatformIOS> WeakSelf = AsShared();

	TapCloudSaveIOSBridge::GetArchiveCover(Uuid, FileId,
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

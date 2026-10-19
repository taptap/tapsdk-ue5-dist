// Copyright TapTap. All Rights Reserved.

#include "Platform/TapCloudSavePlatformDesktop.h"
#include "Platform/TapCloudSaveDesktopToken.h"

#include "TapCloudSave.h"
#include "TapCloudSaveModule.h"
#include "Desktop/TapCloudSaveDesktopWrapper.h"

#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"
#include "Internal/Http/TapHttpCommonParams.h"
#include "TapLogin.h"
#include "Internal/TapLoginInternal.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	// Mirror Unity wrapper docstring:
	//   region: 0=CN, 1=Global, 2=RND, 3=Global RND
	int32 ComputeRegion(const FTapTapSdkOptions& Cfg)
	{
		const bool bGlobal = (Cfg.Region == ETapRegion::Global);
		if (!FTapSdkConfigState::IsRNDEnvironment()) { return bGlobal ? 1 : 0; }
		return bGlobal ? 3 : 2;
	}

	void RunOnGameThread(TFunction<void()> Fn)
	{
		AsyncTask(ENamedThreads::GameThread, MoveTemp(Fn));
	}

	FString JsonObjectToString(const TSharedRef<FJsonObject>& Obj)
	{
		FString Out;
		auto Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj, Writer);
		return Out;
	}

	FString MakeInitConfigJson(const TOptional<FTapAccount>& Account)
	{
		const FTapTapSdkOptions& Cfg = FTapSdkConfigState::GetCurrentConfig();

		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("region"),         ComputeRegion(Cfg));
		Obj->SetNumberField(TEXT("log_to_console"), Cfg.bEnableLog ? 1 : 0);
		Obj->SetNumberField(TEXT("log_level"),      3);

		const FString DataDir = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("cloudsave_") + Cfg.ClientID);
		Obj->SetStringField(TEXT("data_dir"),       DataDir);

		Obj->SetStringField(TEXT("client_id"),      Cfg.ClientID);
		Obj->SetStringField(TEXT("client_token"),   Cfg.ClientToken);
		Obj->SetStringField(TEXT("ua"),             FTapHttpCommonParams::GenerateUserAgent());
		Obj->SetStringField(TEXT("lang"),           TEXT("zh-CN"));
		Obj->SetStringField(TEXT("platform"),       TEXT("PC"));
		Obj->SetStringField(TEXT("device_id"),      FPlatformMisc::GetLoginId());
		Obj->SetStringField(TEXT("sdk_artifact"),   TEXT("UE5"));
		Obj->SetStringField(TEXT("sdk_module_ver"), UTapCloudSave::GetVersion());

		Obj->SetObjectField(TEXT("sdk_token"),
			TapCloudSaveDesktopToken::MakeTokenObject(Account));

		return JsonObjectToString(Obj);
	}

	FString SerializeMeta(const FTapArchiveMetadata& M)
	{
		TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("name"),     M.Name);
		J->SetStringField(TEXT("summary"),  M.Summary);
		J->SetStringField(TEXT("extra"),    M.Extra);
		J->SetNumberField(TEXT("playtime"), M.Playtime);
		return JsonObjectToString(J);
	}

	bool ParseArchive(const TSharedPtr<FJsonObject>& Root, FTapArchiveData& Out)
	{
		if (!Root.IsValid()) return false;
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

	// Cloudsave_sdk unified response:
	//   { "data": {...}, "now": 1234, "success": true }
	// On failure `data` carries { "code": <int>, "msg": <string>, ... }.
	struct FBaseResponse
	{
		bool                    bSuccess = false;
		TSharedPtr<FJsonObject> Data;
	};

	bool ParseBaseResponse(const FString& Json, FBaseResponse& Out)
	{
		TSharedPtr<FJsonObject> Root;
		auto Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
		{
			return false;
		}
		Root->TryGetBoolField(TEXT("success"), Out.bSuccess);
		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		if (Root->TryGetObjectField(TEXT("data"), DataPtr) && DataPtr)
		{
			Out.Data = *DataPtr;
		}
		return true;
	}

	void ParseErrorData(const TSharedPtr<FJsonObject>& Data, int32& Code, FString& Msg)
	{
		Code = -1;
		Msg.Reset();
		if (!Data.IsValid()) return;
		int32 Tmp = -1;
		if (Data->TryGetNumberField(TEXT("code"), Tmp)) { Code = Tmp; }
		Data->TryGetStringField(TEXT("msg"), Msg);
	}

	// Helper: fire an OnError on game thread.
	void FireError(const FTapOnArchiveError& OnError, int32 Code, FString Msg)
	{
		RunOnGameThread([OnError, Code, Msg = MoveTemp(Msg)]
		{
			OnError.ExecuteIfBound(Code, Msg);
		});
	}
}

FTapCloudSavePlatformDesktop::FTapCloudSavePlatformDesktop()
{
	InitCompleteEvent = FPlatformProcess::GetSynchEventFromPool(true);
}

FTapCloudSavePlatformDesktop::~FTapCloudSavePlatformDesktop()
{
	// The platform factory owns this object until all queued API tasks release
	// their shared references, so no queued task can observe a freed event.
	{
		FScopeLock Lock(&InitLock);
		bInitStarted = true;
		bInitComplete = true;
	}
	if (InitCompleteEvent)
	{
		InitCompleteEvent->Trigger();
		FPlatformProcess::ReturnSynchEventToPool(InitCompleteEvent);
		InitCompleteEvent = nullptr;
	}
	FTapLoginInternal::OnLoginStateChanged().RemoveAll(this);
	TapCloudSaveDesktopWrapper::Shutdown();
}

bool FTapCloudSavePlatformDesktop::IsInitialized() const
{
	FScopeLock Lock(&InitLock);
	return bInitNative;
}

bool FTapCloudSavePlatformDesktop::WaitForNativeInitialization()
{
	bool bStartInitialization = false;
	{
		FScopeLock Lock(&InitLock);
		if (bInitComplete)
		{
			return bInitNative;
		}
		bStartInitialization = !bInitStarted;
	}

	if (bStartInitialization)
	{
		// API calls are issued on a background worker, so starting the same
		// asynchronous initialization here preserves the lazy-init fallback.
		Initialize();
	}

	if (!InitCompleteEvent)
	{
		return false;
	}
	InitCompleteEvent->Wait();
	FScopeLock Lock(&InitLock);
	return bInitComplete && bInitNative;
}

bool FTapCloudSavePlatformDesktop::HasCompleteLoginCredentials(
	const TOptional<FTapAccount>& Account) const
{
	return TapCloudSaveDesktopToken::HasCompleteMacCredentials(Account);
}

void FTapCloudSavePlatformDesktop::NotifyState(ETapCloudSaveState State, int32 RawCode)
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

void FTapCloudSavePlatformDesktop::RegisterStateCallback(const FTapOnCloudSaveState& OnState)
{
	FScopeLock Lock(&StateLock);
	for (const auto& Existing : StateCallbacks)
	{
		if (Existing.GetHandle() == OnState.GetHandle())
		{
			UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave] state callback already registered"));
			return;
		}
	}
	StateCallbacks.Add(OnState);
}

void FTapCloudSavePlatformDesktop::UnregisterStateCallback(const FTapOnCloudSaveState& OnState)
{
	FScopeLock Lock(&StateLock);
	const int32 Removed = StateCallbacks.RemoveAll([&](const FTapOnCloudSaveState& E)
	{
		return E.GetHandle() == OnState.GetHandle();
	});
	if (Removed == 0)
	{
		UE_LOG(LogTapCloudSave, Warning, TEXT("[CloudSave] state callback not found"));
	}
}

void FTapCloudSavePlatformDesktop::Initialize()
{
	{
		FScopeLock Lock(&InitLock);
		if (bInitStarted)
		{
			return;
		}
		bInitStarted = true;
	}

	Tracker = FTapOpenlogTracker(TEXT("TapCloudSave"), UTapCloudSave::GetVersion());
	Tracker.TrackInit();

	if (!TapCloudSaveDesktopWrapper::Initialize())
	{
		UE_LOG(LogTapCloudSave, Error, TEXT("[CloudSave-Desktop] native wrapper failed to load"));
		{
			FScopeLock Lock(&InitLock);
			bInitComplete = true;
		}
		if (InitCompleteEvent) InitCompleteEvent->Trigger();
		NotifyState(ETapCloudSaveState::InitFail, TapCloudSaveResultCode::INIT_FAIL);
		return;
	}

	TapCloudSaveDesktopWrapper::SetLogLevel(1, 1);
	FTapLoginInternal::OnLoginStateChanged().RemoveAll(this);
	FTapLoginInternal::OnLoginStateChanged().AddRaw(this, &FTapCloudSavePlatformDesktop::HandleLoginStateChanged);

	TSharedPtr<FTapCloudSavePlatformDesktop> Self = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Self]()
	{
		const TOptional<FTapAccount> Account = FTapLoginInternal::PeekCachedAccount();
		const FString Cfg = MakeInitConfigJson(Account);
		const int Rc = TapCloudSaveDesktopWrapper::CallInitialize(Cfg);
		UE_LOG(LogTapCloudSave, Log, TEXT("TapSdkCloudSaveInitialize rc=%d"), Rc);
		{
			FScopeLock Lock(&Self->InitLock);
			Self->bInitNative = Rc >= 0;
			Self->bInitComplete = true;
		}
		if (Self->InitCompleteEvent) Self->InitCompleteEvent->Trigger();
		if (Rc < 0)
		{
			Self->NotifyState(ETapCloudSaveState::InitFail, TapCloudSaveResultCode::INIT_FAIL);
			return;
		}
		Self->SeedSyncedToken(Account);
	});

	UE_LOG(LogTapCloudSave, Verbose, TEXT("[CloudSave-Desktop] access token will be synchronized before each API call"));
}

bool FTapCloudSavePlatformDesktop::SyncAccessToken()
{
	const TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
	if (!HasCompleteLoginCredentials(Account))
	{
		UE_LOG(LogTapCloudSave, Warning,
			TEXT("[CloudSave-Desktop] cannot synchronize access token: missing kid or MAC key"));
		return false;
	}

	{
		FScopeLock Lock(&TokenLock);
		if (SyncedTokenKid == Account->AccessToken.Kid && SyncedMacKey == Account->AccessToken.MacKey)
		{
			return true;
		}
	}
	return PushAccessToken();
}

void FTapCloudSavePlatformDesktop::SeedSyncedToken(
	const TOptional<FTapAccount>& Account)
{
	FScopeLock Lock(&TokenLock);
	if (HasCompleteLoginCredentials(Account))
	{
		SyncedTokenKid = Account->AccessToken.Kid;
		SyncedMacKey = Account->AccessToken.MacKey;
	}
	else
	{
		SyncedTokenKid.Reset();
		SyncedMacKey.Reset();
	}
}

void FTapCloudSavePlatformDesktop::ResetSyncedToken()
{
	FScopeLock Lock(&TokenLock);
	SyncedTokenKid.Reset();
	SyncedMacKey.Reset();
}

bool FTapCloudSavePlatformDesktop::PushAccessToken()
{
	// The native SDK keeps one process-wide token. Serialize updates so an older
	// login-state callback cannot overtake a newer one in the DLL.
	FScopeLock PushLock(&TokenPushLock);

	// An event snapshot may be stale by the time this background task runs.
	// Re-read the authoritative account after taking the serialization lock so
	// rapid login/logout/account-switch transitions always push the newest state.
	const TOptional<FTapAccount> EffectiveAccount =
		FTapTapLogin::GetCurrentTapAccount();
	const FString TokenJson = JsonObjectToString(
		TapCloudSaveDesktopToken::MakeTokenObject(EffectiveAccount));
	const int Rc = TapCloudSaveDesktopWrapper::UpdateAccessToken(TokenJson);
	UE_LOG(LogTapCloudSave, Log,
		TEXT("[CloudSave-Desktop] UpdateAccessToken rc=%d hasAccount=%d"),
		Rc, EffectiveAccount.IsSet() ? 1 : 0);
	if (Rc < 0)
	{
		ResetSyncedToken();
		return false;
	}

	FScopeLock Lock(&TokenLock);
	if (HasCompleteLoginCredentials(EffectiveAccount))
	{
		SyncedTokenKid = EffectiveAccount->AccessToken.Kid;
		SyncedMacKey = EffectiveAccount->AccessToken.MacKey;
	}
	else
	{
		SyncedTokenKid.Reset();
		SyncedMacKey.Reset();
	}
	return true;
}

void FTapCloudSavePlatformDesktop::HandleLoginStateChanged(const TOptional<FTapAccount>& /*Account*/)
{
	if (!IsInitialized() || !TapCloudSaveDesktopWrapper::IsReady()) return;
	TWeakPtr<FTapCloudSavePlatformDesktop> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [WeakSelf]
	{
		if (TSharedPtr<FTapCloudSavePlatformDesktop> Self = WeakSelf.Pin())
		{
			Self->PushAccessToken();
		}
	});
}

bool FTapCloudSavePlatformDesktop::CheckInitAndLogin(
	FString& OutErrorMsg, int32& OutErrorCode)
{
	if (!WaitForNativeInitialization())
	{
		OutErrorMsg = TEXT("Init or login state check failed: native initialization failed");
		OutErrorCode = TapCloudSaveResultCode::INIT_FAIL;
		return false;
	}

	const TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
	if (!HasCompleteLoginCredentials(Account))
	{
		NotifyState(ETapCloudSaveState::NeedLogin, TapCloudSaveResultCode::NEED_LOGIN);
		OutErrorMsg = TEXT("Init or login state check failed: login credentials are missing or incomplete; please log in again");
		OutErrorCode = TapCloudSaveResultCode::NEED_LOGIN;
		return false;
	}

	if (!SyncAccessToken())
	{
		NotifyState(ETapCloudSaveState::NeedLogin, TapCloudSaveResultCode::NEED_LOGIN);
		OutErrorMsg = TEXT("Init or login state check failed: access token synchronization failed; please log in again");
		OutErrorCode = TapCloudSaveResultCode::NEED_LOGIN;
		return false;
	}
	return true;
}

// ---------------------------------------------------------------------------
// Business APIs
// ---------------------------------------------------------------------------

void FTapCloudSavePlatformDesktop::CreateArchive(
	const FTapArchiveMetadata& Metadata,
	const FString& ArchiveFilePath,
	const FString& CoverFilePath,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("createArchive"), SessionId);
	TWeakPtr<FTapCloudSavePlatformDesktop> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakSelf, Metadata, ArchiveFilePath, CoverFilePath, OnSuccess, OnError, SessionId]
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) return;
		FString Err; int32 ErrCode = -1;
		if (!Self->CheckInitAndLogin(Err, ErrCode)) { FireError(OnError, ErrCode, Err); return; }
		TArray<uint8> FileBytes;
		if (!FFileHelper::LoadFileToArray(FileBytes, *ArchiveFilePath))
		{
			FireError(OnError, -1, TEXT("failed to read archive file")); return;
		}
		TArray<uint8> CoverBytes;
		if (!CoverFilePath.IsEmpty()) FFileHelper::LoadFileToArray(CoverBytes, *CoverFilePath);
		const FString RespJson = TapCloudSaveDesktopWrapper::CallCreateArchive(
			SerializeMeta(Metadata), FileBytes, CoverBytes);
		FBaseResponse Base;
		if (!ParseBaseResponse(RespJson, Base))
		{
			FireError(OnError, -1, FString::Printf(TEXT("createArchive parse failed: %s"), *RespJson)); return;
		}
		if (Base.bSuccess)
		{
			FTapArchiveData Data; ParseArchive(Base.Data, Data);
			Data.Name = Metadata.Name; Data.Summary = Metadata.Summary;
			Data.Extra = Metadata.Extra; Data.Playtime = Metadata.Playtime;
			Self->Tracker.TrackSuccess(TEXT("createArchive"), SessionId);
			RunOnGameThread([OnSuccess, Data] { OnSuccess.ExecuteIfBound(Data); });
		}
		else
		{
			int32 Code = -1; FString Msg; ParseErrorData(Base.Data, Code, Msg);
			Self->Tracker.TrackFailure(TEXT("createArchive"), SessionId, FString::FromInt(Code), Msg);
			FireError(OnError, Code, FString::Printf(TEXT("createArchive failed: %s"), *Msg));
		}
	});
}

void FTapCloudSavePlatformDesktop::UpdateArchive(
	const FString& Uuid,
	const FTapArchiveMetadata& Metadata,
	const FString& ArchiveFilePath,
	const FString& CoverFilePath,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("updateArchive"), SessionId);
	TWeakPtr<FTapCloudSavePlatformDesktop> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakSelf, Uuid, Metadata, ArchiveFilePath, CoverFilePath, OnSuccess, OnError, SessionId]
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) return;
		FString Err; int32 ErrCode = -1;
		if (!Self->CheckInitAndLogin(Err, ErrCode)) { FireError(OnError, ErrCode, Err); return; }
		TArray<uint8> FileBytes;
		if (!FFileHelper::LoadFileToArray(FileBytes, *ArchiveFilePath))
		{
			FireError(OnError, -1, TEXT("failed to read archive file")); return;
		}
		TArray<uint8> CoverBytes;
		if (!CoverFilePath.IsEmpty()) FFileHelper::LoadFileToArray(CoverBytes, *CoverFilePath);
		const FString RespJson = TapCloudSaveDesktopWrapper::CallUpdateArchive(
			Uuid, SerializeMeta(Metadata), FileBytes, CoverBytes);
		FBaseResponse Base;
		if (!ParseBaseResponse(RespJson, Base))
		{
			FireError(OnError, -1, FString::Printf(TEXT("updateArchive parse failed: %s"), *RespJson)); return;
		}
		if (Base.bSuccess)
		{
			FTapArchiveData Data; ParseArchive(Base.Data, Data);
			Data.Name = Metadata.Name; Data.Summary = Metadata.Summary;
			Data.Extra = Metadata.Extra; Data.Playtime = Metadata.Playtime;
			if (Data.Uuid.IsEmpty()) Data.Uuid = Uuid;
			Self->Tracker.TrackSuccess(TEXT("updateArchive"), SessionId);
			RunOnGameThread([OnSuccess, Data] { OnSuccess.ExecuteIfBound(Data); });
		}
		else
		{
			int32 Code = -1; FString Msg; ParseErrorData(Base.Data, Code, Msg);
			Self->Tracker.TrackFailure(TEXT("updateArchive"), SessionId, FString::FromInt(Code), Msg);
			FireError(OnError, Code, FString::Printf(TEXT("updateArchive failed: %s"), *Msg));
		}
	});
}

void FTapCloudSavePlatformDesktop::DeleteArchive(
	const FString& Uuid,
	const FTapOnArchiveResult& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("deleteArchive"), SessionId);
	TWeakPtr<FTapCloudSavePlatformDesktop> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakSelf, Uuid, OnSuccess, OnError, SessionId]
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) return;
		FString Err; int32 ErrCode = -1;
		if (!Self->CheckInitAndLogin(Err, ErrCode)) { FireError(OnError, ErrCode, Err); return; }
		const FString RespJson = TapCloudSaveDesktopWrapper::CallDeleteArchive(Uuid);
		FBaseResponse Base;
		if (!ParseBaseResponse(RespJson, Base))
		{
			FireError(OnError, -1, FString::Printf(TEXT("deleteArchive parse failed: %s"), *RespJson)); return;
		}
		if (Base.bSuccess)
		{
			FTapArchiveData Data; ParseArchive(Base.Data, Data);
			if (Data.Uuid.IsEmpty()) Data.Uuid = Uuid;
			Self->Tracker.TrackSuccess(TEXT("deleteArchive"), SessionId);
			RunOnGameThread([OnSuccess, Data] { OnSuccess.ExecuteIfBound(Data); });
		}
		else
		{
			int32 Code = -1; FString Msg; ParseErrorData(Base.Data, Code, Msg);
			Self->Tracker.TrackFailure(TEXT("deleteArchive"), SessionId, FString::FromInt(Code), Msg);
			FireError(OnError, Code, FString::Printf(TEXT("deleteArchive failed: %s"), *Msg));
		}
	});
}

void FTapCloudSavePlatformDesktop::GetArchiveList(
	const FTapOnArchiveListResult& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("getArchiveList"), SessionId);
	TWeakPtr<FTapCloudSavePlatformDesktop> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakSelf, OnSuccess, OnError, SessionId]
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) return;
		FString Err; int32 ErrCode = -1;
		if (!Self->CheckInitAndLogin(Err, ErrCode)) { FireError(OnError, ErrCode, Err); return; }
		const FString RespJson = TapCloudSaveDesktopWrapper::CallGetArchiveList();
		FBaseResponse Base;
		if (!ParseBaseResponse(RespJson, Base))
		{
			FireError(OnError, -1, FString::Printf(TEXT("getArchiveList parse failed: %s"), *RespJson)); return;
		}
		if (Base.bSuccess)
		{
			TArray<FTapArchiveData> Result;
			const TArray<TSharedPtr<FJsonValue>>* Saves = nullptr;
			if (Base.Data.IsValid() && Base.Data->TryGetArrayField(TEXT("saves"), Saves) && Saves)
			{
				for (const TSharedPtr<FJsonValue>& V : *Saves)
				{
					const TSharedPtr<FJsonObject> Obj = V.IsValid() ? V->AsObject() : nullptr;
					if (!Obj.IsValid()) continue;
					FTapArchiveData D; ParseArchive(Obj, D); Result.Add(MoveTemp(D));
				}
			}
			Self->Tracker.TrackSuccess(TEXT("getArchiveList"), SessionId);
			RunOnGameThread([OnSuccess, Result] { OnSuccess.ExecuteIfBound(Result); });
		}
		else
		{
			int32 Code = -1; FString Msg; ParseErrorData(Base.Data, Code, Msg);
			Self->Tracker.TrackFailure(TEXT("getArchiveList"), SessionId, FString::FromInt(Code), Msg);
			FireError(OnError, Code, FString::Printf(TEXT("getArchiveList failed: %s"), *Msg));
		}
	});
}

void FTapCloudSavePlatformDesktop::GetArchiveData(
	const FString& Uuid,
	const FString& FileId,
	const FTapOnArchiveBinaryResult& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("getArchiveData"), SessionId);
	TWeakPtr<FTapCloudSavePlatformDesktop> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakSelf, Uuid, FileId, OnSuccess, OnError, SessionId]
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) return;
		FString Err; int32 ErrCode = -1;
		if (!Self->CheckInitAndLogin(Err, ErrCode)) { FireError(OnError, ErrCode, Err); return; }
		bool bOk = false; TArray<uint8> Bytes; FString ErrJson;
		TapCloudSaveDesktopWrapper::CallGetArchiveData(Uuid, FileId, bOk, Bytes, ErrJson);
		if (!bOk)
		{
			FBaseResponse Base; int32 Code = -1; FString Msg;
			if (ParseBaseResponse(ErrJson, Base)) ParseErrorData(Base.Data, Code, Msg);
			else Msg = ErrJson.IsEmpty() ? TEXT("unknown error") : ErrJson;
			Self->Tracker.TrackFailure(TEXT("getArchiveData"), SessionId, FString::FromInt(Code), Msg);
			FireError(OnError, Code, FString::Printf(TEXT("getArchiveData failed: %s"), *Msg)); return;
		}
		Self->Tracker.TrackSuccess(TEXT("getArchiveData"), SessionId);
		RunOnGameThread([OnSuccess, Bytes] { OnSuccess.ExecuteIfBound(Bytes); });
	});
}

void FTapCloudSavePlatformDesktop::GetArchiveCover(
	const FString& Uuid,
	const FString& FileId,
	const FTapOnArchiveBinaryResult& OnSuccess,
	const FTapOnArchiveError& OnError)
{
	const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Tracker.TrackStart(TEXT("getArchiveCover"), SessionId);
	TWeakPtr<FTapCloudSavePlatformDesktop> WeakSelf = AsShared();
	AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask,
		[WeakSelf, Uuid, FileId, OnSuccess, OnError, SessionId]
	{
		auto Self = WeakSelf.Pin();
		if (!Self.IsValid()) return;
		FString Err; int32 ErrCode = -1;
		if (!Self->CheckInitAndLogin(Err, ErrCode)) { FireError(OnError, ErrCode, Err); return; }
		bool bOk = false; TArray<uint8> Bytes; FString ErrJson;
		TapCloudSaveDesktopWrapper::CallGetArchiveCover(Uuid, FileId, bOk, Bytes, ErrJson);
		if (!bOk)
		{
			FBaseResponse Base; int32 Code = -1; FString Msg;
			if (ParseBaseResponse(ErrJson, Base)) ParseErrorData(Base.Data, Code, Msg);
			else Msg = ErrJson.IsEmpty() ? TEXT("unknown error") : ErrJson;
			Self->Tracker.TrackFailure(TEXT("getArchiveCover"), SessionId, FString::FromInt(Code), Msg);
			FireError(OnError, Code, FString::Printf(TEXT("getArchiveCover failed: %s"), *Msg)); return;
		}
		Self->Tracker.TrackSuccess(TEXT("getArchiveCover"), SessionId);
		RunOnGameThread([OnSuccess, Bytes] { OnSuccess.ExecuteIfBound(Bytes); });
	});
}

// Copyright TapTap. All Rights Reserved.
#include "TapAchievementDesktop.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC

#include "TapAchievementCallbackManager.h"
#include "TapAchievementResultParser.h"
#include "TapAchievementModule.h"
#include "TapAchievementToastManager.h"
#include "TapCore.h"
#include "Internal/TapCoreInternal.h"
#include "Internal/Http/TapHttp.h"
#include "Internal/Http/TapHttpResult.h"
#include "Internal/Http/TapHttpException.h"
#include "Internal/Http/TapHttpErrorConstants.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "TapBasicTypes.h"
#include "TapLogin.h"

static TUniquePtr<FTapAchievementDesktop> GDesktop;

namespace
{
    using TapAchievementResultParser::ParseAchievementBean;

    int32 MapHttpError(const TSharedPtr<FTapHttpException>& Ex)
    {
        if (!Ex.IsValid()) return ETapAchievementError::UnknownError;
        switch (Ex->GetErrorType())
        {
        case ETapHttpErrorType::NetworkError:
        case ETapHttpErrorType::Timeout:
            return ETapAchievementError::NetworkError;
        case ETapHttpErrorType::ServerError:
        {
            auto ServerEx = StaticCastSharedPtr<FTapHttpServerException>(Ex);
            if (ServerEx.IsValid())
            {
                const FString& ErrCode = ServerEx->GetErrorData().Error;
                if (ErrCode == FTapHttpErrorConstants::ERROR_ACCESS_DENIED ||
                    ErrCode == FTapHttpErrorConstants::ERROR_FORBIDDEN)
                    return ETapAchievementError::AccessDenied;
                if (ErrCode == FTapHttpErrorConstants::ERROR_INVALID_REQUEST ||
                    ErrCode == FTapHttpErrorConstants::ERROR_INVALID_PARAMETER ||
                    ErrCode == FTapHttpErrorConstants::ERROR_NOT_FOUND)
                    return ETapAchievementError::InvalidRequest;
            }
            return ETapAchievementError::UnknownError;
        }
        default:
            return ETapAchievementError::UnknownError;
        }
    }

    bool ShouldRetainPendingItem(const TSharedPtr<FTapHttpException>& Ex)
    {
        // 对齐 Android：仅确定性的服务端错误删除缓存；网络、超时、解析及未知错误均保留重试。
        return !Ex.IsValid() || Ex->GetErrorType() != ETapHttpErrorType::ServerError;
    }
}

bool FTapAchievementPendingQueue::Enqueue(const FTapAchievementPendingItem& Item)
{
    // 对齐 Android：Unlock 以 achievementId 为 key，同用户重复调用覆盖为一条；
    // Increment 以每次调用的 UUID 为 key，因此始终保留独立条目。
    if (Item.Type == 1)
    {
        for (FTapAchievementPendingItem& Existing : Items)
        {
            if (Existing.Type == 1 &&
                Existing.UserId == Item.UserId &&
                Existing.AchievementId == Item.AchievementId)
            {
                if (UploadingUUIDs.Contains(Existing.UUID))
                {
                    // 旧请求仍在上传时，保持同一个缓存 key；回调会按该 key 完成删除或重试。
                    // Android Unlock 同样以 achievementId 为 key，因此新调用不会新增第二条。
                    return false;
                }
                Existing = Item;
                return true;
            }
        }
    }

    Items.Add(Item);
    return true;
}

void FTapAchievementPendingQueue::AddLoaded(const FTapAchievementPendingItem& Item)
{
    Items.Add(Item);
}

TArray<FTapAchievementPendingItem> FTapAchievementPendingQueue::BeginReplay(const FString& UserId)
{
    TArray<FTapAchievementPendingItem> ToReplay;
    for (const FTapAchievementPendingItem& Item : Items)
    {
        if (Item.UserId == UserId && !UploadingUUIDs.Contains(Item.UUID))
        {
            UploadingUUIDs.Add(Item.UUID);
            ToReplay.Add(Item);
        }
    }
    return ToReplay;
}

void FTapAchievementPendingQueue::FinishReplay(const FString& UUID, bool bRemoveItem)
{
    UploadingUUIDs.Remove(UUID);
    if (bRemoveItem)
    {
        Items.RemoveAll([&UUID](const FTapAchievementPendingItem& Item)
        {
            return Item.UUID == UUID;
        });
    }
}

FTapAchievementDesktop& FTapAchievementDesktop::Get()
{
    if (!GDesktop.IsValid())
    {
        GDesktop = MakeUnique<FTapAchievementDesktop>();
    }
    return *GDesktop;
}

void FTapAchievementDesktop::Initialize(const FTapTapSdkOptions& Config)
{
    Tracker = FTapOpenlogTracker(TEXT("TapAchievement"), TapAchievement_VERSION);
    HttpClient = FTapHttp::NewBuilder(TEXT("TapAchievement"), TapAchievement_VERSION)
        ->Parser(FTapHttpParser::CreateEnvelopeParser(true))->Build();
    LoadQueueFromDisk();
    FlushPendingQueue();
    bInitialized = true;
    Tracker.TrackInit();
    UE_LOG(LogTapAchievement, Log, TEXT("TapAchievement initialized, pending queue: %d items"),
           PendingQueue.Num());
}

bool FTapAchievementDesktop::CheckPreconditions(const FString& AchievementId, FString& OutUserId)
{
    if (!bInitialized)
    {
		FTapAchievementCallbackManager::Get().Failure(
            AchievementId, ETapAchievementError::NotInitialized,
            TEXT("Currently not initialized, please initialize first."));
        return false;
    }
    if (FTapHttp::GetRegion() != ETapRegion::CN)
    {
		FTapAchievementCallbackManager::Get().Failure(
            AchievementId, ETapAchievementError::RegionNotSupported,
            TEXT("Current RegionType not supported, only support TapTapRegionType.CN"));
        return false;
    }
    {
        TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
        if (!Account.IsSet() || Account->OpenID.IsEmpty() || !Account->AccessToken.IsValidForMac())
        {
            FTapAchievementCallbackManager::Get().Failure(
                AchievementId, ETapAchievementError::NotLogged,
                TEXT("Currently not logged in, please login first."));
            return false;
        }
        OutUserId = Account->OpenID;
    }
    if (AchievementId.IsEmpty())
    {
        FTapAchievementCallbackManager::Get().Failure(
            AchievementId, ETapAchievementError::InvalidRequest,
            TEXT("AchievementId cannot be empty"));
        return false;
    }
    return true;
}

void FTapAchievementDesktop::SetToastEnable(bool bEnable)
{
    bToastEnabled = bEnable;
}

FString FTapAchievementDesktop::GetQueueFilePath() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!QueueFilePathForTest.IsEmpty()) return QueueFilePathForTest;
#endif
    return FPaths::Combine(FPaths::ProjectSavedDir(),
                           TEXT("TapAchievement"), TEXT("pending_queue.json"));
}

// ── Task 6: Unlock + Increment HTTP ──────────────────────────────────────────

void FTapAchievementDesktop::Unlock(const FString& AchievementId)
{
    FString UserId;
    if (!CheckPreconditions(AchievementId, UserId)) return;
    FlushPendingQueue();
    FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Tracker.TrackStart(TEXT("unlock"), SessionId, {
        {TEXT("achievement_id"), AchievementId}
    });
    DoUnlock(AchievementId, SessionId, UserId, false);
}

bool FTapAchievementDesktop::CanSendForUser(const FString& UserId) const
{
    const TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
    return !UserId.IsEmpty() && Account.IsSet() && Account->OpenID == UserId &&
        Account->AccessToken.IsValidForMac();
}

void FTapAchievementDesktop::DoUnlock(const FString& AchievementId,
                                       const FString& SessionId,
                                       const FString& UserId,
                                       bool bFromQueue)
{
    const FTapHttpJsonResultDelegate Callback = CreateUnlockCallback(AchievementId, SessionId, UserId, bFromQueue);
    // Flush/replay may synchronously invoke game callbacks that switch the account.
    // Keep the operation for its original owner instead of sending with another user's MAC.
    if (!CanSendForUser(UserId))
    {
        Callback.ExecuteIfBound(FTapHttpJsonResult::UnknownError(TEXT("Achievement account changed before request could be sent")));
        return;
    }
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("achievement_id"), AchievementId);

    HttpClient->PostJson(
        TEXT("achievement/v1/unlock"),
        TMap<FString, FString>(),
        Body,
        true,
        nullptr,
        Callback);
}

FTapHttpJsonResultDelegate FTapAchievementDesktop::CreateUnlockCallback(
    const FString& AchievementId, const FString& SessionId, const FString& UserId, bool bFromQueue)
{
    return FTapHttpJsonResultDelegate::CreateLambda(
            [this, AchievementId, SessionId, UserId, bFromQueue](const FTapHttpJsonResult& Result)
            {
                if (Result.IsSuccess())
                {
                    if (bFromQueue)
                    {
                        FinishPendingReplay(SessionId, true);
                    }

                    TSharedPtr<FJsonObject> Data = Result.GetData();
                    if (!Data.IsValid()) { return; }

                    Tracker.TrackSuccess(TEXT("unlock"), SessionId, {
                        {TEXT("achievement_id"), AchievementId}
                    });

                    // 统一用 TryGetObjectField + TryGetBoolField，避免字段缺失时 crash
                    const TSharedPtr<FJsonObject>* AchBeanPtr = nullptr;
                    bool bNewlyUnlocked = false;
                    if (Data->TryGetObjectField(TEXT("achievement"), AchBeanPtr) &&
                        AchBeanPtr && AchBeanPtr->IsValid() &&
                        (*AchBeanPtr)->TryGetBoolField(TEXT("newly_unlocked"), bNewlyUnlocked) &&
                        bNewlyUnlocked)
                    {
                        FTapAchievementResult R;
                        if (ParseAchievementBean(*AchBeanPtr, ETapAchievementType::Normal, R))
                        {
	FTapAchievementCallbackManager::Get().Success(
                                ETapAchievementError::UnlockSuccess, R);
                            if (bToastEnabled)
                                FTapAchievementToastManager::Get().Enqueue(R);
                        }
                    }
                    const TSharedPtr<FJsonObject>* PlatBeanPtr = nullptr;
                    bool bPlatNewlyUnlocked = false;
                    if (Data->TryGetObjectField(TEXT("platinum_achievement"), PlatBeanPtr) &&
                        PlatBeanPtr && PlatBeanPtr->IsValid() &&
                        (*PlatBeanPtr)->TryGetBoolField(TEXT("newly_unlocked"), bPlatNewlyUnlocked) &&
                        bPlatNewlyUnlocked)
                    {
                        FTapAchievementResult PR;
                        if (ParseAchievementBean(*PlatBeanPtr, ETapAchievementType::Platinum, PR))
                        {
                            FTapAchievementCallbackManager::Get().Success(
                                ETapAchievementError::UnlockSuccess, PR);
                            if (bToastEnabled)
                                FTapAchievementToastManager::Get().Enqueue(PR);
                        }
                    }
                }
                else
                {
                    int32 ErrCode = MapHttpError(Result.GetException());
                    FString ErrMsg = Result.GetException().IsValid()
                                     ? Result.GetException()->GetMessage()
                                     : TEXT("Unknown error");
                    Tracker.TrackFailure(TEXT("unlock"), SessionId,
                        FString::FromInt(ErrCode), ErrMsg, {
                            {TEXT("achievement_id"), AchievementId}
                        });
                    FTapAchievementCallbackManager::Get().Failure(AchievementId, ErrCode, ErrMsg);
                    if (bFromQueue)
                    {
                        FinishPendingReplay(SessionId, !ShouldRetainPendingItem(Result.GetException()));
                    }
                    if (bFromQueue && !ShouldRetainPendingItem(Result.GetException()))
                    {
                        UE_LOG(LogTapAchievement, Warning,
                               TEXT("Pending item '%s' permanently dropped after replay failure (code=%d)"),
                               *AchievementId, ErrCode);
                    }
                    if (!bFromQueue && ShouldRetainPendingItem(Result.GetException()))
                    {
                        FTapAchievementPendingItem Item;
                        Item.AchievementId = AchievementId;
                        Item.Steps         = 0;
                        Item.Type          = 1;
                        Item.UUID          = SessionId;
                        Item.TimestampMs   = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
                        Item.UserId        = UserId;
                        EnqueuePending(Item);
                    }
                }
            });
}

void FTapAchievementDesktop::Increment(const FString& AchievementId, int32 Step)
{
    FString UserId;
    if (!CheckPreconditions(AchievementId, UserId)) return;
    if (Step <= 0)
    {
        FTapAchievementCallbackManager::Get().Failure(
            AchievementId, ETapAchievementError::InvalidRequest,
            TEXT("Step must be greater than 0"));
        return;
    }
    FlushPendingQueue();
    FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Tracker.TrackStart(TEXT("increment"), SessionId, {
        {TEXT("achievement_id"), AchievementId}
    });
    DoIncrement(AchievementId, Step, SessionId, UserId, false);
}

void FTapAchievementDesktop::DoIncrement(const FString& AchievementId,
                                          int32 Step,
                                          const FString& SessionId,
                                          const FString& UserId,
                                          bool bFromQueue)
{
    const FTapHttpJsonResultDelegate Callback = CreateIncrementCallback(AchievementId, Step, SessionId, UserId, bFromQueue);
    if (!CanSendForUser(UserId))
    {
        Callback.ExecuteIfBound(FTapHttpJsonResult::UnknownError(TEXT("Achievement account changed before request could be sent")));
        return;
    }
    TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("achievement_id"), AchievementId);
    Body->SetNumberField(TEXT("steps"), Step);

    HttpClient->PostJson(
        TEXT("achievement/v1/increment"),
        TMap<FString, FString>(),
        Body,
        true,
        nullptr,
        Callback);
}

FTapHttpJsonResultDelegate FTapAchievementDesktop::CreateIncrementCallback(
    const FString& AchievementId, int32 Step, const FString& SessionId, const FString& UserId, bool bFromQueue)
{
    return FTapHttpJsonResultDelegate::CreateLambda(
            [this, AchievementId, Step, SessionId, UserId, bFromQueue](const FTapHttpJsonResult& Result)
            {
                if (Result.IsSuccess())
                {
                    if (bFromQueue)
                    {
                        FinishPendingReplay(SessionId, true);
                    }

                    TSharedPtr<FJsonObject> Data = Result.GetData();
                    if (!Data.IsValid()) { return; }

                    Tracker.TrackSuccess(TEXT("increment"), SessionId, {
                        {TEXT("achievement_id"), AchievementId}
                    });

                    const TSharedPtr<FJsonObject>* IncrAchBeanPtr = nullptr;
                    FTapAchievementResult R;
                    if (Data->TryGetObjectField(TEXT("achievement"), IncrAchBeanPtr) &&
                        IncrAchBeanPtr && IncrAchBeanPtr->IsValid() &&
                        ParseAchievementBean(*IncrAchBeanPtr, ETapAchievementType::Normal, R))
                    {
                        FTapAchievementCallbackManager::Get().Success(
                            ETapAchievementError::IncrementSuccess, R);
                        bool bIncrNewlyUnlocked = false;
                        if ((*IncrAchBeanPtr)->TryGetBoolField(TEXT("newly_unlocked"), bIncrNewlyUnlocked) &&
                            bIncrNewlyUnlocked)
                        {
                            FTapAchievementCallbackManager::Get().Success(
                                ETapAchievementError::UnlockSuccess, R);
                            if (bToastEnabled)
                                FTapAchievementToastManager::Get().Enqueue(R);
                        }
                    }
                    const TSharedPtr<FJsonObject>* PlatBeanPtr = nullptr;
                    bool bPlatNewlyUnlocked = false;
                    if (Data->TryGetObjectField(TEXT("platinum_achievement"), PlatBeanPtr) &&
                        PlatBeanPtr && PlatBeanPtr->IsValid() &&
                        (*PlatBeanPtr)->TryGetBoolField(TEXT("newly_unlocked"), bPlatNewlyUnlocked) &&
                        bPlatNewlyUnlocked)
                    {
                        FTapAchievementResult PR;
                        if (ParseAchievementBean(*PlatBeanPtr, ETapAchievementType::Platinum, PR))
                        {
                            FTapAchievementCallbackManager::Get().Success(
                                ETapAchievementError::UnlockSuccess, PR);
                            if (bToastEnabled)
                                FTapAchievementToastManager::Get().Enqueue(PR);
                        }
                    }
                }
                else
                {
                    int32 ErrCode = MapHttpError(Result.GetException());
                    FString ErrMsg = Result.GetException().IsValid()
                                     ? Result.GetException()->GetMessage()
                                     : TEXT("Unknown error");
                    Tracker.TrackFailure(TEXT("increment"), SessionId,
                        FString::FromInt(ErrCode), ErrMsg, {
                            {TEXT("achievement_id"), AchievementId}
                        });
                    FTapAchievementCallbackManager::Get().Failure(AchievementId, ErrCode, ErrMsg);
                    if (bFromQueue)
                    {
                        FinishPendingReplay(SessionId, !ShouldRetainPendingItem(Result.GetException()));
                    }
                    if (bFromQueue && !ShouldRetainPendingItem(Result.GetException()))
                    {
                        UE_LOG(LogTapAchievement, Warning,
                               TEXT("Pending item '%s' permanently dropped after replay failure (code=%d)"),
                               *AchievementId, ErrCode);
                    }
                    if (!bFromQueue && ShouldRetainPendingItem(Result.GetException()))
                    {
                        FTapAchievementPendingItem Item;
                        Item.AchievementId = AchievementId;
                        Item.Steps         = Step;
                        Item.Type          = 0;
                        Item.UUID          = SessionId;
                        Item.TimestampMs   = FDateTime::UtcNow().ToUnixTimestamp() * 1000;
                        Item.UserId        = UserId;
                        EnqueuePending(Item);
                    }
                }
            });
}

// ── Task 7: 离线队列持久化与重放 ──────────────────────────────────────────────

void FTapAchievementDesktop::ShowAchievements()
{
    if (FTapHttp::GetRegion() != ETapRegion::CN)
    {
        FTapAchievementCallbackManager::Get().Failure(
            TEXT(""), ETapAchievementError::RegionNotSupported,
            TEXT("Current RegionType not supported, only support TapTapRegionType.CN"));
        return;
    }
    if (!bInitialized)
    {
        FTapAchievementCallbackManager::Get().Failure(
            TEXT(""), ETapAchievementError::NotInitialized,
            TEXT("Currently not initialized, please initialize first."));
        return;
    }
    {
        TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
        if (!Account.IsSet() || Account->OpenID.IsEmpty() || !Account->AccessToken.IsValidForMac())
        {
            FTapAchievementCallbackManager::Get().Failure(
                TEXT(""), ETapAchievementError::NotLogged,
                TEXT("Currently not logged in, please login first."));
            return;
        }
    }

    const FString SessionId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Tracker.TrackStart(TEXT("showAchievements"), SessionId);

    FString Url = FTapCoreInternal::GetGatekeeperConfigUrl(TEXT("achievement_my_list_url"));
    if (Url.IsEmpty())
    {
        const FString ErrorMsg = TEXT("achievement_my_list_url not available");
        UE_LOG(LogTapAchievement, Warning,
               TEXT("ShowAchievements: %s (gatekeeper may not have completed yet)"), *ErrorMsg);
        Tracker.TrackFailure(
            TEXT("showAchievements"), SessionId,
            FString::FromInt(ETapAchievementError::UnknownError), ErrorMsg);
        return;
    }

    UE_LOG(LogTapAchievement, Log, TEXT("ShowAchievements: opening %s"), *Url);
    FString LaunchError;
    FPlatformProcess::LaunchURL(*Url, nullptr, &LaunchError);
    if (LaunchError.IsEmpty())
    {
        Tracker.TrackSuccess(TEXT("showAchievements"), SessionId);
    }
    else
    {
        UE_LOG(LogTapAchievement, Warning, TEXT("ShowAchievements: %s"), *LaunchError);
        Tracker.TrackFailure(
            TEXT("showAchievements"), SessionId,
            FString::FromInt(ETapAchievementError::UnknownError), LaunchError);
    }
}

void FTapAchievementDesktop::EnqueuePending(const FTapAchievementPendingItem& Item)
{
    if (!PendingQueue.Enqueue(Item))
    {
        UE_LOG(LogTapAchievement, Verbose,
               TEXT("Offline unlock already uploading: %s"), *Item.AchievementId);
        return;
    }

    SaveQueueToDisk();
    UE_LOG(LogTapAchievement, Log, TEXT("Queued offline: %s (type=%d)"),
           *Item.AchievementId, Item.Type);
}

void FTapAchievementDesktop::FinishPendingReplay(const FString& UUID, bool bRemoveItem)
{
    PendingQueue.FinishReplay(UUID, bRemoveItem);
    SaveQueueToDisk();
}

void FTapAchievementDesktop::SaveQueueToDisk() const
{
    TArray<TSharedPtr<FJsonValue>> JsonArr;
    for (const FTapAchievementPendingItem& Item : PendingQueue.GetItems())
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("achievement_id"), Item.AchievementId);
        Obj->SetNumberField(TEXT("steps"),          Item.Steps);
        Obj->SetNumberField(TEXT("type"),           Item.Type);
        Obj->SetStringField(TEXT("uuid"),           Item.UUID);
        Obj->SetNumberField(TEXT("timestamp_ms"),   (double)Item.TimestampMs);
        Obj->SetStringField(TEXT("user_id"),        Item.UserId);
        JsonArr.Add(MakeShared<FJsonValueObject>(Obj));
    }
    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    FJsonSerializer::Serialize(JsonArr, Writer);
    FString FilePath = GetQueueFilePath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(FilePath), true);
    FFileHelper::SaveStringToFile(JsonStr, *FilePath);
}

void FTapAchievementDesktop::LoadQueueFromDisk()
{
    FString FilePath = GetQueueFilePath();
    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *FilePath)) return;

    TArray<TSharedPtr<FJsonValue>> JsonArr;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, JsonArr))
    {
        UE_LOG(LogTapAchievement, Warning, TEXT("Failed to parse pending queue, discarding"));
        return;
    }
    for (const TSharedPtr<FJsonValue>& Val : JsonArr)
    {
        TSharedPtr<FJsonObject> Obj = Val->AsObject();
        if (!Obj.IsValid()) continue;
        FTapAchievementPendingItem Item;
        Item.AchievementId = Obj->GetStringField(TEXT("achievement_id"));
        Item.Steps         = (int32)Obj->GetNumberField(TEXT("steps"));
        Item.Type          = (int32)Obj->GetNumberField(TEXT("type"));
        Item.UUID          = Obj->GetStringField(TEXT("uuid"));
        Item.TimestampMs   = (int64)Obj->GetNumberField(TEXT("timestamp_ms"));
        Obj->TryGetStringField(TEXT("user_id"), Item.UserId);
        PendingQueue.AddLoaded(Item);
    }
    UE_LOG(LogTapAchievement, Log, TEXT("Loaded %d pending items from disk"), PendingQueue.Num());
}

void FTapAchievementDesktop::FlushPendingQueue()
{
    if (PendingQueue.Num() == 0) return;
    if (!FTapHttp::IsInitialized()) return;
    if (FTapHttp::GetRegion() != ETapRegion::CN) return;

    TOptional<FTapAccount> Account = FTapTapLogin::GetCurrentTapAccount();
    if (!Account.IsSet() || Account->OpenID.IsEmpty() || !Account->AccessToken.IsValidForMac()) return;
    const FString CurrentUserId = Account->OpenID;

    TArray<FTapAchievementPendingItem> ToReplay = PendingQueue.BeginReplay(CurrentUserId);
    if (ToReplay.Num() == 0) return;

    UE_LOG(LogTapAchievement, Log, TEXT("Flushing %d pending achievement(s) for user %s"),
           ToReplay.Num(), *CurrentUserId);
    for (const FTapAchievementPendingItem& Item : ToReplay)
    {
        if (Item.Type == 1)
            DoUnlock(Item.AchievementId, Item.UUID, Item.UserId, true);
        else
            DoIncrement(Item.AchievementId, Item.Steps, Item.UUID, Item.UserId, true);
    }
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC

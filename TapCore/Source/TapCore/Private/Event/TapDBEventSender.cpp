// Copyright TapTap. All Rights Reserved.

#include "Internal/TapDBEventSender.h"
#include "Internal/Storage/TapSharedCacheFile.h"
#include "Internal/TapLogPrivacy.h"
#include "TapDBEventSenderUtils.h"
#include "Internal/TapDBConstants.h"
#include "TapCoreModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

FTapDBEventSender::FTapDBEventSender()
	: bIsInitialized(false)
	, bIsShuttingDown(false)
{
	AliveFlag = MakeShared<bool, ESPMode::ThreadSafe>(true);
}

FTapDBEventSender::~FTapDBEventSender()
{
	Shutdown();
}

void FTapDBEventSender::Init(const FString& InClientID, ETapRegion Region, const FString& CachePath, bool bPrivateFile)
{
	ClientID = InClientID;
	bPrivate = bPrivateFile;
	bIsShuttingDown = false;
	AliveFlag = MakeShared<bool, ESPMode::ThreadSafe>(true);

	if (Region == ETapRegion::CN)
	{
		ServerUrl = TapDBConstants::SERVER_URL_CN + TapDBConstants::API_PATH;
	}
	else
	{
		ServerUrl = TapDBConstants::SERVER_URL_IO + TapDBConstants::API_PATH;
	}

	// Setup cache file path
	FString Dir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("TapDB"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Dir))
	{
		PlatformFile.CreateDirectoryTree(*Dir);
	}
	EventCacheFilePath = FPaths::Combine(Dir, FString::Printf(TEXT("tapdb_events_%s.json"), *ClientID));

	if (!CachePath.IsEmpty()) EventCacheFilePath = CachePath;
	bSavePending = true;

	// Load previously unsent events
	bCacheLoaded = LoadEvents();

	// Setup periodic send timer (15s interval)
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateLambda([this](float DeltaTime) -> bool
		{
			OnTimerTick();
			return true; // Keep ticking
		}),
		TapDBConstants::SEND_INTERVAL_SECONDS
	);

	bIsInitialized = true;
	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Initialized, ServerUrl=%s, CacheFile=%s"),
		*ServerUrl, *EventCacheFilePath);
}

void FTapDBEventSender::Send(TSharedPtr<FJsonObject> EventData)
{
	if (!bIsInitialized || bIsShuttingDown || !EventData.IsValid())
	{
		return;
	}

	int32 QueueCount = 0;
	{
		FScopeLock ScopeLock(&QueueLock);
		EventQueue.Add(EventData);
		QueueCount = EventQueue.Num();
	}

	SaveEvents();

	UE_LOG(LogTapDB, Verbose, TEXT("[TapDBEventSender] Event enqueued, queue size: %d"), QueueCount);

	// Trigger batch send on every new event (aligned with Android behavior)
	TrySendBatch();
}

void FTapDBEventSender::Flush()
{
	if (!bIsInitialized)
	{
		return;
	}

	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Flush requested"));
	SaveEvents(true);
	TrySendBatch();
}

void FTapDBEventSender::ClearCache()
{
	{
		FScopeLock ScopeLock(&QueueLock);
		EventQueue.Empty();
	}

	bCacheLoaded = true; // Explicit invalidation may replace even an unreadable cache.
	SaveEvents();

	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Cache cleared — machine changed"));
}

void FTapDBEventSender::Shutdown()
{
	if (!bIsInitialized)
	{
		return;
	}

	bIsShuttingDown = true;

	// Invalidate alive flag to prevent HTTP callbacks from accessing this
	if (AliveFlag.IsValid())
	{
		*AliveFlag = false;
	}

	// Remove ticker
	if (TickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	// Recover in-flight batches back to queue head
	{
		FScopeLock ScopeLock(&QueueLock);
		for (auto& Pair : InFlightBatches)
		{
			EventQueue.Insert(Pair.Value, 0);
		}
		InFlightBatches.Empty();
	}

	// Save remaining events to disk
	SaveEvents();

	bIsInitialized = false;
	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Shutdown, saved remaining events"));
}

bool FTapDBEventSender::LoadEvents(bool bUnderLock)
{
	if (EventCacheFilePath.IsEmpty())
	{
		return false;
	}

	auto Lease = (bPrivate || bUnderLock) ? TUniquePtr<FTapSharedCacheFile::FLease>() : FTapSharedCacheFile::TryAcquire(EventCacheFilePath);
	if (!bPrivate && !bUnderLock && !Lease) return false;
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *EventCacheFilePath))
	{
		return !IFileManager::Get().FileExists(*EventCacheFilePath);
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] Failed to parse cached events"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* DataArray;
	if (Root->TryGetArrayField(TEXT("data"), DataArray))
	{
		for (const auto& Val : *DataArray)
			if (!Val || Val->Type != EJson::Object) return false;
		FScopeLock ScopeLock(&QueueLock);
		for (const TSharedPtr<FJsonValue>& Val : *DataArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Object)
			{
				EventQueue.Add(Val->AsObject());
			}
		}
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Loaded %d cached events"), EventQueue.Num());
		return true;
	}
	return false;
}

void FTapDBEventSender::SaveEvents(bool bOnlyIfPending)
{
	FScopeLock PersistenceLock(&SaveLock);
	if (bOnlyIfPending && !bSavePending) return;
	bSavePending = true;
	if (EventCacheFilePath.IsEmpty())
	{
		return;
	}
	auto Lease = bPrivate ? TUniquePtr<FTapSharedCacheFile::FLease>() : FTapSharedCacheFile::TryAcquire(EventCacheFilePath);
	if (!bPrivate && !Lease) return; // Retry at the next existing timer/Flush, without queued tasks.
	if (!bCacheLoaded)
	{
		bCacheLoaded = LoadEvents(true);
		if (!bCacheLoaded) return; // Never overwrite a queue that failed to load.
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> DataArray;

	{
		FScopeLock ScopeLock(&QueueLock);
		for (const TSharedPtr<FJsonObject>& Event : EventQueue)
		{
			DataArray.Add(MakeShared<FJsonValueObject>(Event));
		}
		for (const auto& Pair : InFlightBatches)
			for (const auto& Event : Pair.Value) DataArray.Add(MakeShared<FJsonValueObject>(Event));
	}
	Root->SetArrayField(TEXT("data"), DataArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	if (!FTapSharedCacheFile::WriteUnderLock(EventCacheFilePath, OutputString))
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] Failed to save events to %s"), *EventCacheFilePath);
	}
	else
	{
		bSavePending = false;
	}
}

bool FTapDBEventSender::TrySendBatch()
{
	// A concurrent save must not observe the gap between dequeue and in-flight registration.
	FScopeLock PersistenceLock(&SaveLock);
	if (!bCacheLoaded || bIsShuttingDown) return false;
	TArray<TSharedPtr<FJsonObject>> Batch;

	{
		FScopeLock ScopeLock(&QueueLock);
		if (EventQueue.Num() == 0)
		{
			return false;
		}

		int32 Count = FMath::Min(EventQueue.Num(), TapDBConstants::MAX_BATCH_SIZE);
		for (int32 i = 0; i < Count; ++i)
		{
			Batch.Add(EventQueue[i]);
		}
		EventQueue.RemoveAt(0, Count);
	}

	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Sending batch of %d events"), Batch.Num());
	DoHttpSend(Batch, 0);
	return true;
}

void FTapDBEventSender::OnTimerTick()
{
	if (!bIsInitialized || bIsShuttingDown)
	{
		return;
	}

	Flush();
}

void FTapDBEventSender::DoHttpSend(const TArray<TSharedPtr<FJsonObject>>& Batch, int32 Attempt)
{
	if (Batch.Num() == 0)
	{
		return;
	}

	// Build request body: {"data": [event1, event2, ...]}
	// Strip internal _retry_count field from a copy so the original event keeps
	// its retry count for subsequent failure callbacks and persisted retries.
	TSharedPtr<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> DataArray;
	for (const TSharedPtr<FJsonObject>& Event : Batch)
	{
		DataArray.Add(MakeShared<FJsonValueObject>(
			TapDBEventSenderUtils::CreateRequestEvent(Event)));
	}
	RequestBody->SetArrayField(TEXT("data"), DataArray);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

	UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] HTTP POST body (%d events):\n%s"), Batch.Num(), *TapLogPrivacy::Body(BodyString));

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
	HttpRequest->SetURL(ServerUrl);
	HttpRequest->SetVerb(TEXT("POST"));
	HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	HttpRequest->SetContentAsString(BodyString);

	// Register batch as in-flight
	int32 BatchId;
	{
		FScopeLock ScopeLock(&QueueLock);
		BatchId = NextBatchId++;
		InFlightBatches.Add(BatchId, Batch);
	}

	SaveEvents(); // Persist queued AND in-flight records until a response acknowledges them.
	TSharedPtr<bool, ESPMode::ThreadSafe> AliveFlagCopy = AliveFlag;

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, AliveFlagCopy, BatchId, Attempt](
			FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!AliveFlagCopy.IsValid() || !(*AliveFlagCopy))
			{
				return;
			}

			// Keep removal and retry/requeue in one persistence transaction as well.
			FScopeLock PersistenceLock(&SaveLock);
			TArray<TSharedPtr<FJsonObject>> CompletedBatch;
			{
				FScopeLock ScopeLock(&QueueLock);
				InFlightBatches.RemoveAndCopyValue(BatchId, CompletedBatch);
			}

			int32 ResponseCode = Response.IsValid() ? Response->GetResponseCode() : 0;
			OnHttpResponseReceived(bConnectedSuccessfully, ResponseCode, MoveTemp(CompletedBatch), Attempt);
		}
	);

	if (!HttpRequest->ProcessRequest())
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] ProcessRequest failed"));
		TArray<TSharedPtr<FJsonObject>> FailedBatch;
		{
			FScopeLock ScopeLock(&QueueLock);
			InFlightBatches.RemoveAndCopyValue(BatchId, FailedBatch);
		}
		OnHttpResponseReceived(false, 0, MoveTemp(FailedBatch), Attempt);
	}
}

void FTapDBEventSender::OnHttpResponseReceived(bool bSuccess, int32 ResponseCode,
	TArray<TSharedPtr<FJsonObject>> SentEvents, int32 Attempt)
{
	if (bSuccess && ResponseCode == 200)
	{
		SaveEvents();
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Batch sent successfully (%d events)"), SentEvents.Num());
		return;
	}

	if (ResponseCode == 400)
	{
		SaveEvents();
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] Batch rejected (400), discarding %d events"), SentEvents.Num());
		return;
	}

	UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] Batch failed (code=%d, connected=%s, attempt=%d/%d) for %d events"),
		ResponseCode, bSuccess ? TEXT("true") : TEXT("false"), Attempt + 1, TapDBConstants::MAX_RETRY + 1, SentEvents.Num());

	if (Attempt < TapDBConstants::MAX_RETRY)
	{
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Immediate retry %d/%d for %d events"),
			Attempt + 1, TapDBConstants::MAX_RETRY, SentEvents.Num());
		DoHttpSend(SentEvents, Attempt + 1);
		return;
	}

	UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] Immediate retries exhausted, keeping %d events for the next send trigger"),
		SentEvents.Num());
	{
		FScopeLock ScopeLock(&QueueLock);
		EventQueue.Insert(SentEvents, 0);
	}
	SaveEvents();
}

void FTapDBEventSender::SendHistory(const TArray<TSharedPtr<FJsonObject>>& Events, ETapRegion Region, TFunction<void(bool)> Complete)
{
	struct FHistory : TSharedFromThis<FHistory>
	{
		TArray<TSharedPtr<FJsonObject>> Events;
		FString Url;
		TFunction<void(bool)> Complete;
		int32 Offset = 0;
		void Next()
		{
			if (Offset == Events.Num()) { Complete(true); return; }
			const int32 Count = FMath::Min(200, Events.Num() - Offset);
			auto Body = MakeShared<FJsonObject>();
			TArray<TSharedPtr<FJsonValue>> Batch;
			for (int32 I = 0; I < Count; ++I)
				Batch.Add(MakeShared<FJsonValueObject>(TapDBEventSenderUtils::CreateRequestEvent(Events[Offset + I])));
			Body->SetArrayField(TEXT("data"), Batch);
			FString Json;
			FJsonSerializer::Serialize(Body, TJsonWriterFactory<>::Create(&Json));
			auto Request = FHttpModule::Get().CreateRequest();
			Request->SetURL(Url); Request->SetVerb(TEXT("POST"));
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			Request->SetContentAsString(Json);
			auto Self = AsShared();
			auto Done = MakeShared<bool>(false);
			auto Finish = [Self, Done, Count](bool bSuccess)
			{
				if (*Done) return;
				*Done = true;
				if (!bSuccess) { Self->Complete(false); return; }
				Self->Offset += Count;
				Self->Next();
			};
			Request->OnProcessRequestComplete().BindLambda([Finish](FHttpRequestPtr, FHttpResponsePtr Response, bool bSuccess)
			{
				Finish(bSuccess && Response.IsValid() && Response->GetResponseCode() == 200);
			});
			if (!Request->ProcessRequest()) Finish(false);
		}
	};
	auto History = MakeShared<FHistory>();
	History->Events = Events;
	History->Url = (Region == ETapRegion::CN ? TapDBConstants::SERVER_URL_CN : TapDBConstants::SERVER_URL_IO) + TapDBConstants::API_PATH;
	History->Complete = MoveTemp(Complete);
	History->Next();
}

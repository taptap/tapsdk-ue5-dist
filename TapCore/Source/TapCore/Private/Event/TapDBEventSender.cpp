// Copyright TapTap. All Rights Reserved.

#include "Internal/TapDBEventSender.h"
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

void FTapDBEventSender::Init(const FString& InClientID, ETapRegion Region)
{
	ClientID = InClientID;

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

	// Load previously unsent events
	LoadEvents();

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
	TrySendBatch();
}

void FTapDBEventSender::ClearCache()
{
	{
		FScopeLock ScopeLock(&QueueLock);
		EventQueue.Empty();
	}

	if (!EventCacheFilePath.IsEmpty())
	{
		IFileManager::Get().Delete(*EventCacheFilePath);
	}

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

void FTapDBEventSender::LoadEvents()
{
	if (EventCacheFilePath.IsEmpty())
	{
		return;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *EventCacheFilePath))
	{
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] Failed to parse cached events"));
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* DataArray;
	if (Root->TryGetArrayField(TEXT("data"), DataArray))
	{
		FScopeLock ScopeLock(&QueueLock);
		for (const TSharedPtr<FJsonValue>& Val : *DataArray)
		{
			if (Val.IsValid() && Val->Type == EJson::Object)
			{
				EventQueue.Add(Val->AsObject());
			}
		}
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Loaded %d cached events"), EventQueue.Num());
	}
}

void FTapDBEventSender::SaveEvents()
{
	if (EventCacheFilePath.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> DataArray;

	{
		FScopeLock ScopeLock(&QueueLock);
		for (const TSharedPtr<FJsonObject>& Event : EventQueue)
		{
			DataArray.Add(MakeShared<FJsonValueObject>(Event));
		}
	}

	Root->SetArrayField(TEXT("data"), DataArray);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(OutputString, *EventCacheFilePath))
	{
		UE_LOG(LogTapDB, Warning, TEXT("[TapDBEventSender] Failed to save events to %s"), *EventCacheFilePath);
	}
}

bool FTapDBEventSender::TrySendBatch()
{
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

	SaveEvents();

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

	TrySendBatch();
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

	TSharedPtr<bool, ESPMode::ThreadSafe> AliveFlagCopy = AliveFlag;

	HttpRequest->OnProcessRequestComplete().BindLambda(
		[this, AliveFlagCopy, BatchId, Attempt](
			FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
		{
			if (!AliveFlagCopy.IsValid() || !(*AliveFlagCopy))
			{
				return;
			}

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
		UE_LOG(LogTapDB, Log, TEXT("[TapDBEventSender] Batch sent successfully (%d events)"), SentEvents.Num());
		return;
	}

	if (ResponseCode == 400)
	{
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

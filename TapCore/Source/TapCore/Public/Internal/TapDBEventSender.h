// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Containers/Ticker.h"
#include "TapBasicTypes.h"

/**
 * Manages event queue, disk persistence, and batch HTTP upload.
 * Events are queued in memory, persisted to disk, and sent in batches
 * to the TapDB server at regular intervals or when queue threshold is reached.
 */
class TAPCORE_API FTapDBEventSender
{
public:
	FTapDBEventSender();
	~FTapDBEventSender();

	void Init(const FString& ClientID, ETapRegion Region);
	void Send(TSharedPtr<FJsonObject> EventData);
	void Flush();
	void ClearCache();
	void Shutdown();

private:
	void LoadEvents();
	void SaveEvents();
	bool TrySendBatch();
	void OnTimerTick();
	void DoHttpSend(const TArray<TSharedPtr<FJsonObject>>& Batch, int32 Attempt = 0);
	void OnHttpResponseReceived(bool bSuccess, int32 ResponseCode,
		TArray<TSharedPtr<FJsonObject>> SentEvents, int32 Attempt);

	TArray<TSharedPtr<FJsonObject>> EventQueue;
	FString EventCacheFilePath;
	FString ServerUrl;
	FString ClientID;
	bool bIsInitialized;
	bool bIsShuttingDown;

	FCriticalSection QueueLock;
	FTSTicker::FDelegateHandle TickerHandle;

	/** In-flight batches keyed by a monotonic BatchId for shutdown recovery */
	int32 NextBatchId = 0;
	TMap<int32, TArray<TSharedPtr<FJsonObject>>> InFlightBatches;

	/** Shared flag for HTTP callback safety — prevents use-after-free */
	TSharedPtr<bool, ESPMode::ThreadSafe> AliveFlag;
};

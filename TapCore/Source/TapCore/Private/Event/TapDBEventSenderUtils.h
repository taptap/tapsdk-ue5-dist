// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Internal/TapDBConstants.h"
#include "TapJsonCompat.h"

namespace TapDBEventSenderUtils
{
	inline TSharedPtr<FJsonObject> CreateRequestEvent(const TSharedPtr<FJsonObject>& Event)
	{
		if (!Event.IsValid())
		{
			return nullptr;
		}

		TSharedPtr<FJsonObject> RequestEvent = MakeShared<FJsonObject>();
		for (const auto& Pair : Event->Values)
		{
			const FString Key = TapJson::KeyString(Pair.Key);
			if (Key != TEXT("_retry_count"))
			{
				RequestEvent->SetField(Key, Pair.Value);
			}
		}
		return RequestEvent;
	}

	inline bool PrepareRetry(const TSharedPtr<FJsonObject>& Event)
	{
		if (!Event.IsValid())
		{
			return false;
		}

		const int32 RetryCount = Event->HasField(TEXT("_retry_count"))
			? Event->GetIntegerField(TEXT("_retry_count")) + 1
			: 1;
		if (RetryCount > TapDBConstants::MAX_RETRY)
		{
			return false;
		}

		Event->SetNumberField(TEXT("_retry_count"), RetryCount);
		return true;
	}
}

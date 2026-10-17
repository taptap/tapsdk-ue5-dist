#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace TapCompliancePaymentResult
{
	inline void Dispatch(const FString& Data,
		const TFunction<void(int32, const FString&, const FString&)>& OnResult,
		const TFunction<void(const FString&)>& OnError)
	{
		TSharedPtr<FJsonObject> Object;
		auto Reader = TJsonReaderFactory<>::Create(Data);
		double Status = -1;
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid() ||
			!Object->TryGetNumberField(TEXT("status"), Status) || (Status != 0 && Status != 1))
		{
			if (OnError) OnError(TEXT("Invalid payment check response"));
			return;
		}
		FString Title, Description;
		Object->TryGetStringField(TEXT("title"), Title);
		Object->TryGetStringField(TEXT("description"), Description);
		if (OnResult) OnResult(static_cast<int32>(Status), Title, Description);
	}
}

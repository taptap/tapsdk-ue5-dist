// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Internal/TapAndroidCallbackId.h"
#include "Misc/ScopeLock.h"
#include "Serialization/JsonSerializer.h"

/** Pending Android route calls. A request owns its callback and JNI reference cleanup. */
class FTapComplianceAsyncRequests
{
public:
	int64 Add(TFunction<void(const FString&)> Callback)
	{
		FScopeLock Guard(&Lock);
		const int64 Id = FTapAndroidCallbackId::Allocate();
		Requests.Add(Id, FRequest{MoveTemp(Callback), {}});
		return Id;
	}

	void AttachCleanup(int64 Id, TFunction<void()> Cleanup)
	{
		{
			FScopeLock Guard(&Lock);
			if (FRequest* Request = Requests.Find(Id))
			{
				Request->Cleanup = MoveTemp(Cleanup);
				return;
			}
		}
		// A completion that won the race still releases a subsequently attached reference.
		if (Cleanup) Cleanup();
	}

	bool Complete(int64 Id, const FString& Response)
	{
		FRequest Request;
		{
			FScopeLock Guard(&Lock);
			if (!Requests.RemoveAndCopyValue(Id, Request)) return false;
		}
		// Cleanup and callbacks may re-enter the registry; never invoke them under its lock.
		if (Request.Cleanup) Request.Cleanup();
		if (Request.Callback) Request.Callback(Response);
		return true;
	}

	void Fail(int64 Id, const FString& Message)
	{
		auto Error = MakeShared<FJsonObject>();
		Error->SetBoolField(TEXT("success"), false);
		Error->SetStringField(TEXT("message"), Message);
		FString Response;
		auto Writer = TJsonWriterFactory<>::Create(&Response);
		FJsonSerializer::Serialize(Error, Writer);
		Complete(Id, Response);
	}

	void HandleImmediateResponse(int64 Id, const FString& Response)
	{
		TSharedPtr<FJsonObject> Object;
		auto Reader = TJsonReaderFactory<>::Create(Response);
		bool bSuccess = false;
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid() ||
			!Object->HasTypedField<EJson::Boolean>(TEXT("success")) ||
			!Object->TryGetBoolField(TEXT("success"), bSuccess))
		{
			Fail(Id, TEXT("Invalid immediate response from TapCompliance route"));
			return;
		}
		bool bAsync = false;
		if (bSuccess && Object->HasTypedField<EJson::Boolean>(TEXT("async")) &&
			Object->TryGetBoolField(TEXT("async"), bAsync) && bAsync) return;
		Complete(Id, Response);
	}

private:
	struct FRequest
	{
		TFunction<void(const FString&)> Callback;
		TFunction<void()> Cleanup;
	};
	FCriticalSection Lock;
	TMap<int64, FRequest> Requests;
};

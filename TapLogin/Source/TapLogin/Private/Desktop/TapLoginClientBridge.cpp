// Copyright TapTap. All Rights Reserved.
#include "TapLoginClientBridge.h"

#if PLATFORM_WINDOWS

#include "Internal/Launcher/TapLauncherDllWrapper.h"
#include "Launcher/TapLauncherTypes.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "taptap_api.h"

namespace
{
	FCriticalSection GLock;
	bool GbInFlight = false;
	bool GbCallbackRegistered = false;
	FTapLoginClientAuthorizeComplete GCompletion;

	void ClearRequestLocked()
	{
		GbInFlight = false;
		GbCallbackRegistered = false;
		GCompletion.Unbind();
	}

	void OnAuthorizeFinished(int32 EventId, void* UserData)
	{
		if (EventId != TapLauncherEventId::AuthorizeFinishedInternal || !UserData)
		{
			return;
		}

		const auto* Response = static_cast<const TapAuthorizeFinishedInternal*>(UserData);
		int32 Length = 0;
		while (Length < UE_ARRAY_COUNT(Response->callback_uri) && Response->callback_uri[Length] != '\0')
		{
			++Length;
		}

		FTapLoginClientAuthorizeComplete Completion;
		bool bCancelled = false;
		FString RedirectUri;
		{
			FScopeLock Lock(&GLock);
			if (!GbInFlight)
			{
				return;
			}

			bCancelled = Response->is_cancel != 0;
			if (Length < UE_ARRAY_COUNT(Response->callback_uri))
			{
				RedirectUri = UTF8_TO_TCHAR(Response->callback_uri);
			}
			else
			{
				UE_LOG(LogTemp, Warning, TEXT("[TapLoginClient] Unterminated authorization callback"));
				RedirectUri = TEXT("native authorization callback was malformed");
			}

			if (GbCallbackRegistered)
			{
				TapLauncherDllWrapper::UnregisterCallback(
					TapLauncherEventId::AuthorizeFinishedInternal,
					&OnAuthorizeFinished);
			}
			Completion = MoveTemp(GCompletion);
			ClearRequestLocked();
		}

		if (Completion.IsBound())
		{
			AsyncTask(ENamedThreads::GameThread,
				[Completion = MoveTemp(Completion), bCancelled, RedirectUri]() mutable
			{
				Completion.ExecuteIfBound(bCancelled, RedirectUri);
			});
		}
	}
}

bool FTapLoginClientBridge::IsAvailable()
{
	return TapLauncherDllWrapper::IsLoaded()
		&& TapLauncherDllWrapper::HasAsyncAuthorizeInternal()
		&& TapLauncherDllWrapper::HasCallbackExports();
}

bool FTapLoginClientBridge::Start(
	const FString& ScopeStrings,
	const FString& ResponseType,
	const FString& RedirectUri,
	const FString& CodeChallenge,
	const FString& State,
	const FString& CodeChallengeMethod,
	const FString& VersionCode,
	const FString& SdkUa,
	const FString& Info,
	FTapLoginClientAuthorizeComplete OnComplete)
{
	if (!IsAvailable() || !OnComplete.IsBound())
	{
		return false;
	}

	{
		FScopeLock Lock(&GLock);
		if (GbInFlight)
		{
			return false;
		}

		GCompletion = MoveTemp(OnComplete);
		GbInFlight = true;
		TapLauncherDllWrapper::RegisterCallback(
			TapLauncherEventId::AuthorizeFinishedInternal,
			&OnAuthorizeFinished);
		GbCallbackRegistered = true;
	}

	const int32 Result = TapLauncherDllWrapper::AsyncAuthorizeInternal(
		ScopeStrings, ResponseType, RedirectUri, CodeChallenge, State,
		CodeChallengeMethod, VersionCode, SdkUa, Info);
	if (Result == TapUser_AsyncAuthorize_OK)
	{
		return true;
	}

	FScopeLock Lock(&GLock);
	if (GbInFlight)
	{
		if (GbCallbackRegistered)
		{
			TapLauncherDllWrapper::UnregisterCallback(
				TapLauncherEventId::AuthorizeFinishedInternal,
				&OnAuthorizeFinished);
		}
		ClearRequestLocked();
	}
	return false;
}

void FTapLoginClientBridge::Cancel()
{
	FScopeLock Lock(&GLock);
	if (GbCallbackRegistered)
	{
		TapLauncherDllWrapper::UnregisterCallback(
			TapLauncherEventId::AuthorizeFinishedInternal,
			&OnAuthorizeFinished);
	}
	ClearRequestLocked();
}

void FTapLoginClientBridge::Shutdown()
{
	Cancel();
}

#endif // PLATFORM_WINDOWS

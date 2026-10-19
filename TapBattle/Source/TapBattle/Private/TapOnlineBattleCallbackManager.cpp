// Copyright TapTap. All Rights Reserved.
#include "TapOnlineBattleCallbackManager.h"

#include "Async/Async.h"
#include "Misc/ScopeLock.h"

FTapOnlineBattleCallbackManager& FTapOnlineBattleCallbackManager::Get()
{
	static FTapOnlineBattleCallbackManager Instance;
	return Instance;
}

void FTapOnlineBattleCallbackManager::Register(const TScriptInterface<ITapOnlineBattleCallback>& Callback)
{
	UObject* Object = Callback.GetObject();
	if (!IsValid(Object) || !Object->GetClass()->ImplementsInterface(UTapOnlineBattleCallback::StaticClass()))
	{
		return;
	}

	FScopeLock Lock(&CallbackLock);
	Callbacks.RemoveAll([](const TWeakObjectPtr<UObject>& Item) { return !Item.IsValid(); });
	if (!Callbacks.ContainsByPredicate([Object](const TWeakObjectPtr<UObject>& Item) { return Item.Get() == Object; }))
	{
		Callbacks.Add(Object);
	}
}

void FTapOnlineBattleCallbackManager::Unregister(const TScriptInterface<ITapOnlineBattleCallback>& Callback)
{
	UObject* Object = Callback.GetObject();
	FScopeLock Lock(&CallbackLock);
	Callbacks.RemoveAll([Object](const TWeakObjectPtr<UObject>& Item)
	{
		return !Item.IsValid() || Item.Get() == Object;
	});
}

void FTapOnlineBattleCallbackManager::Reset()
{
	FScopeLock Lock(&CallbackLock);
	Callbacks.Reset();
	++Generation;
}

void FTapOnlineBattleCallbackManager::Dispatch(TFunction<void(UObject*)> Invocation)
{
	if (IsInGameThread())
	{
		DispatchOnGameThread(MoveTemp(Invocation));
		return;
	}
	uint64 DispatchGeneration = 0;
	{
		FScopeLock Lock(&CallbackLock);
		DispatchGeneration = Generation;
	}
	AsyncTask(ENamedThreads::GameThread, [this, DispatchGeneration, Invocation = MoveTemp(Invocation)]() mutable
	{
		{
			FScopeLock Lock(&CallbackLock);
			if (DispatchGeneration != Generation)
			{
				return;
			}
		}
		DispatchOnGameThread(MoveTemp(Invocation));
	});
}

void FTapOnlineBattleCallbackManager::DispatchOnGameThread(TFunction<void(UObject*)> Invocation)
{
	TArray<TWeakObjectPtr<UObject>> Snapshot;
	{
		FScopeLock Lock(&CallbackLock);
		Callbacks.RemoveAll([](const TWeakObjectPtr<UObject>& Item) { return !Item.IsValid(); });
		Snapshot = Callbacks;
	}
	for (const TWeakObjectPtr<UObject>& Item : Snapshot)
	{
		if (UObject* Object = Item.Get())
		{
			Invocation(Object);
		}
	}
}

void FTapOnlineBattleCallbackManager::DispatchDisconnected(int32 Code, const FString& Message)
{
	Dispatch([Code, Message](UObject* Object) { ITapOnlineBattleCallback::Execute_OnDisconnected(Object, Code, Message); });
}

void FTapOnlineBattleCallbackManager::DispatchBattleServiceError()
{
	Dispatch([](UObject* Object) { ITapOnlineBattleCallback::Execute_OnBattleServiceError(Object); });
}

void FTapOnlineBattleCallbackManager::DispatchRoomPropertiesChanged(const FString& RoomId, const FString& Name, const FString& CustomProperties)
{
	Dispatch([RoomId, Name, CustomProperties](UObject* Object) { ITapOnlineBattleCallback::Execute_OnRoomPropertiesChanged(Object, RoomId, Name, CustomProperties); });
}

void FTapOnlineBattleCallbackManager::DispatchPlayerCustomPropertiesChanged(const FString& PlayerId, const FString& Properties)
{
	Dispatch([PlayerId, Properties](UObject* Object) { ITapOnlineBattleCallback::Execute_OnPlayerCustomPropertiesChanged(Object, PlayerId, Properties); });
}

void FTapOnlineBattleCallbackManager::DispatchPlayerCustomStatusChanged(const FString& PlayerId, int32 Status)
{
	Dispatch([PlayerId, Status](UObject* Object) { ITapOnlineBattleCallback::Execute_OnPlayerCustomStatusChanged(Object, PlayerId, Status); });
}

void FTapOnlineBattleCallbackManager::DispatchFrameSyncStarted(const FTapBattleFrameSyncInfo& FrameSyncInfo)
{
	Dispatch([FrameSyncInfo](UObject* Object) { ITapOnlineBattleCallback::Execute_OnFrameSyncStarted(Object, FrameSyncInfo); });
}

void FTapOnlineBattleCallbackManager::DispatchFrameReceived(const FTapBattleFrameData& FrameData)
{
	Dispatch([FrameData](UObject* Object) { ITapOnlineBattleCallback::Execute_OnFrameReceived(Object, FrameData); });
}

void FTapOnlineBattleCallbackManager::DispatchFrameSyncStopped(const FString& RoomId, int32 FrameSyncId, int32 Reason)
{
	Dispatch([RoomId, FrameSyncId, Reason](UObject* Object) { ITapOnlineBattleCallback::Execute_OnFrameSyncStopped(Object, RoomId, FrameSyncId, Reason); });
}

void FTapOnlineBattleCallbackManager::DispatchPlayerOffline(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId)
{
	Dispatch([RoomId, RoomOwnerId, PlayerId](UObject* Object) { ITapOnlineBattleCallback::Execute_OnPlayerOffline(Object, RoomId, RoomOwnerId, PlayerId); });
}

void FTapOnlineBattleCallbackManager::DispatchPlayerLeft(const FString& RoomId, const FString& RoomOwnerId, const FString& PlayerId)
{
	Dispatch([RoomId, RoomOwnerId, PlayerId](UObject* Object) { ITapOnlineBattleCallback::Execute_OnPlayerLeft(Object, RoomId, RoomOwnerId, PlayerId); });
}

void FTapOnlineBattleCallbackManager::DispatchPlayerEntered(const FString& RoomId, const FTapBattlePlayerInfo& PlayerInfo)
{
	Dispatch([RoomId, PlayerInfo](UObject* Object) { ITapOnlineBattleCallback::Execute_OnPlayerEntered(Object, RoomId, PlayerInfo); });
}

void FTapOnlineBattleCallbackManager::DispatchCustomMessageReceived(const FString& PlayerId, const FString& Message)
{
	Dispatch([PlayerId, Message](UObject* Object) { ITapOnlineBattleCallback::Execute_OnCustomMessageReceived(Object, PlayerId, Message); });
}

void FTapOnlineBattleCallbackManager::DispatchPlayerKicked(const FString& RoomId, const FString& PlayerId)
{
	Dispatch([RoomId, PlayerId](UObject* Object) { ITapOnlineBattleCallback::Execute_OnPlayerKicked(Object, RoomId, PlayerId); });
}

#if WITH_DEV_AUTOMATION_TESTS
int32 FTapOnlineBattleCallbackManager::GetRegisteredCallbackCountForTest()
{
	FScopeLock Lock(&CallbackLock);
	Callbacks.RemoveAll([](const TWeakObjectPtr<UObject>& Item) { return !Item.IsValid(); });
	return Callbacks.Num();
}
#endif

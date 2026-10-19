// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"

/** Internal helper for persistent UObject callback APIs. */
class FTapWeakCallbackRegistry
{
public:
	void Register(UObject* Callback, UClass* InterfaceClass)
	{
		if (!IsValid(Callback) || !Callback->GetClass()->ImplementsInterface(InterfaceClass)) return;
		FScopeLock Lock(&Mutex);
		Callbacks.RemoveAll([](const TWeakObjectPtr<UObject>& Item) { return !Item.IsValid(); });
		if (!Callbacks.ContainsByPredicate([Callback](const TWeakObjectPtr<UObject>& Item) { return Item.Get() == Callback; }))
		{
			Callbacks.Add(Callback);
		}
	}

	void Unregister(UObject* Callback)
	{
		FScopeLock Lock(&Mutex);
		Callbacks.RemoveAll([Callback](const TWeakObjectPtr<UObject>& Item) { return !Item.IsValid() || Item.Get() == Callback; });
	}

	void Reset()
	{
		FScopeLock Lock(&Mutex);
		Callbacks.Reset();
		++Generation;
	}

	void Dispatch(TFunction<void(UObject*)> Invocation)
	{
		uint64 DispatchGeneration;
		{
			FScopeLock Lock(&Mutex);
			DispatchGeneration = Generation;
		}
		auto Run = [this, DispatchGeneration, Invocation = MoveTemp(Invocation)]() mutable
		{
			TArray<TWeakObjectPtr<UObject>> Snapshot;
			{
				FScopeLock Lock(&Mutex);
				if (DispatchGeneration != Generation) return;
				Callbacks.RemoveAll([](const TWeakObjectPtr<UObject>& Item) { return !Item.IsValid(); });
				Snapshot = Callbacks;
			}
			for (const TWeakObjectPtr<UObject>& Item : Snapshot)
			{
				if (UObject* Object = Item.Get()) Invocation(Object);
			}
		};
		if (IsInGameThread()) Run(); else AsyncTask(ENamedThreads::GameThread, MoveTemp(Run));
	}

	int32 Num()
	{
		FScopeLock Lock(&Mutex);
		Callbacks.RemoveAll([](const TWeakObjectPtr<UObject>& Item) { return !Item.IsValid(); });
		return Callbacks.Num();
	}

	/** Detached snapshot of live receivers; does not retain or mutate registrations. */
	TArray<TWeakObjectPtr<UObject>> Snapshot()
	{
		FScopeLock Lock(&Mutex);
		TArray<TWeakObjectPtr<UObject>> Result;
		for (const TWeakObjectPtr<UObject>& Callback : Callbacks)
		{
			if (Callback.IsValid()) Result.Add(Callback);
		}
		return Result;
	}

private:
	FCriticalSection Mutex;
	TArray<TWeakObjectPtr<UObject>> Callbacks;
	uint64 Generation = 0;
};

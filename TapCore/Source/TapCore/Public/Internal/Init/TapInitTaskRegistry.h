// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internal/Init/TapInitTask.h"

/**
 * TapSDK Initialization Task Registry (Singleton)
 * Manages all registered initialization tasks
 */
class TAPCORE_API FTapInitTaskRegistry
{
public:
	/** Get singleton instance (defined in cpp to keep one copy across DLLs) */
	static FTapInitTaskRegistry& Get();

	/** Register a task */
	void Register(ITapInitTask* Task);

	/** Get all registered tasks */
	TMap<FName, ITapInitTask*>& GetAllTasks()
	{
		return Tasks;
	}

	/** Get all registered tasks (const) */
	const TMap<FName, ITapInitTask*>& GetAllTasks() const
	{
		return Tasks;
	}

	/** Check if a task is registered */
	bool IsTaskRegistered(FName TaskID) const
	{
		return Tasks.Contains(TaskID);
	}

private:
	FTapInitTaskRegistry() = default;
	~FTapInitTaskRegistry() = default;

	// Non-copyable
	FTapInitTaskRegistry(const FTapInitTaskRegistry&) = delete;
	FTapInitTaskRegistry& operator=(const FTapInitTaskRegistry&) = delete;

	TMap<FName, ITapInitTask*> Tasks;
	FCriticalSection CriticalSection;
};

/**
 * Auto Register Implementation
 */
template<typename T>
inline FTapInitTaskAutoRegister<T>::FTapInitTaskAutoRegister()
{
	static T Instance;
	FTapInitTaskRegistry::Get().Register(&Instance);
}

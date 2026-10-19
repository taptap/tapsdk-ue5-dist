// Copyright TapTap. All Rights Reserved.

#include "Internal/Init/TapInitTaskRegistry.h"

FTapInitTaskRegistry& FTapInitTaskRegistry::Get()
{
	static FTapInitTaskRegistry Instance;
	return Instance;
}

void FTapInitTaskRegistry::Register(ITapInitTask* Task)
{
	if (!Task)
	{
		UE_LOG(LogTapCore, Error, TEXT("Cannot register null task"));
		return;
	}

	FScopeLock Lock(&CriticalSection);

	FName TaskID = Task->GetTaskID();
	if (Tasks.Contains(TaskID))
	{
		UE_LOG(LogTapCore, Warning, TEXT("Task '%s' is already registered"), *TaskID.ToString());
		return;
	}

	Tasks.Add(TaskID, Task);
	UE_LOG(LogTapCore, Verbose, TEXT("Registered task: %s"), *TaskID.ToString());
}

// Copyright TapTap. All Rights Reserved.

#include "Init/TapInitTaskExecutor.h"
#include "Internal/Init/TapInitTaskRegistry.h"
#include "HAL/PlatformTime.h"

bool FTapInitTaskExecutor::ExecuteAll(const FTapTapSdkOptions& Config)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_TapInitTaskExecutor_ExecuteAll);

	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("TapSDK Initialization Started"));
	UE_LOG(LogTapCore, Log, TEXT("========================================"));

	double StartTime = FPlatformTime::Seconds();

	// 1. Get all registered tasks
	TMap<FName, ITapInitTask*> FilteredTasks;
	FilterTasks(FTapInitTaskRegistry::Get().GetAllTasks(), FilteredTasks);

	if (FilteredTasks.Num() == 0)
	{
		UE_LOG(LogTapCore, Warning, TEXT("No initialization tasks registered"));
		return true;
	}

	UE_LOG(LogTapCore, Log, TEXT("Found %d initialization task(s)"), FilteredTasks.Num());

	// 2. Build DAG
	FTapInitTaskGraph Graph;
	if (!Graph.BuildDAG(FilteredTasks))
	{
		UE_LOG(LogTapCore, Error, TEXT("Failed to build dependency graph"));
		return false;
	}

	// 3. Topological sort
	TArray<FTapInitTaskNode*> SortedNodes;
	if (!Graph.TopologicalSort(SortedNodes))
	{
		UE_LOG(LogTapCore, Error, TEXT("Failed to perform topological sort"));
		return false;
	}

	// 4. Stable sort by priority
	Graph.StableSortByPriority(SortedNodes);

	// 5. Log execution plan
	UE_LOG(LogTapCore, Log, TEXT("Execution plan:"));
	for (int32 i = 0; i < SortedNodes.Num(); i++)
	{
		FTapInitTaskNode* Node = SortedNodes[i];
		ITapInitTask* Task = Node->Task;

		FString DepsStr;
		if (Node->Dependencies.Num() > 0)
		{
			for (FTapInitTaskNode* Dep : Node->Dependencies)
			{
				if (!DepsStr.IsEmpty())
				{
					DepsStr += TEXT(", ");
				}
				DepsStr += Dep->Task->GetTaskID().ToString();
			}
			DepsStr = FString::Printf(TEXT(" [Depends on: %s]"), *DepsStr);
		}

		UE_LOG(LogTapCore, Log, TEXT("  %d. %s (Priority=%d, Background=%s)%s"),
			i + 1,
			*Task->GetDisplayName(),
			Task->GetPriority(),
			Task->CanExecuteInBackground() ? TEXT("Yes") : TEXT("No"),
			*DepsStr);
	}

	// 6. Execute tasks in parallel
	FGraphEventArray AllCompletionEvents;
	AllCompletionEvents.Reserve(SortedNodes.Num());

	for (FTapInitTaskNode* Node : SortedNodes)
	{
		AllCompletionEvents.Add(ExecuteTask(Node, Config));
	}

	// 7. Wait for all tasks to complete
	FTaskGraphInterface::Get().WaitUntilTasksComplete(
		AllCompletionEvents,
		ENamedThreads::GameThread
	);

	double Duration = (FPlatformTime::Seconds() - StartTime) * 1000.0;

	UE_LOG(LogTapCore, Log, TEXT("========================================"));
	UE_LOG(LogTapCore, Log, TEXT("TapSDK Initialization Completed"));
	UE_LOG(LogTapCore, Log, TEXT("Total time: %.2fms, Tasks: %d"), Duration, SortedNodes.Num());
	UE_LOG(LogTapCore, Log, TEXT("========================================"));

	return true;
}

FGraphEventRef FTapInitTaskExecutor::ExecuteTask(FTapInitTaskNode* Node, const FTapTapSdkOptions& Config)
{
	ITapInitTask* Task = Node->Task;

	// Collect dependency completion events
	FGraphEventArray Prerequisites;
	for (FTapInitTaskNode* Dep : Node->Dependencies)
	{
		if (Dep->CompletionEvent.IsValid())
		{
			Prerequisites.Add(Dep->CompletionEvent);
		}
	}

	// Determine execution thread
	ENamedThreads::Type ExecutionThread = Task->CanExecuteInBackground()
		? ENamedThreads::AnyBackgroundThreadNormalTask
		: ENamedThreads::GameThread;

	// Create and dispatch task
	FGraphEventRef CompletionEvent = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[Task, Config]()
		{
			double TaskStartTime = FPlatformTime::Seconds();

			UE_LOG(LogTapCore, Log, TEXT(">>> Executing: %s"), *Task->GetDisplayName());

			// Execute the task
			Task->Execute(Config);

			double TaskDuration = (FPlatformTime::Seconds() - TaskStartTime) * 1000.0;
			UE_LOG(LogTapCore, Log, TEXT("<<< Completed: %s (%.2fms)"),
				*Task->GetDisplayName(), TaskDuration);
		},
		TStatId(),
		&Prerequisites,
		ExecutionThread
	);

	Node->CompletionEvent = CompletionEvent;
	return CompletionEvent;
}

void FTapInitTaskExecutor::FilterTasks(const TMap<FName, ITapInitTask*>& Tasks,
	TMap<FName, ITapInitTask*>& OutFilteredTasks)
{
	for (const auto& Pair : Tasks)
	{
		ITapInitTask* Task = Pair.Value;

		// Filter development-only tasks in shipping builds
#if UE_BUILD_SHIPPING
		if (Task->IsDevOnly())
		{
			UE_LOG(LogTapCore, Verbose, TEXT("Skipping dev-only task: %s"), *Task->GetDisplayName());
			continue;
		}
#endif

		OutFilteredTasks.Add(Pair.Key, Task);
	}
}

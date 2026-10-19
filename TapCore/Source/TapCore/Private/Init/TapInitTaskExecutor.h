// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "Init/TapInitTaskGraph.h"

/**
 * TapSDK Initialization Task Executor
 * Executes initialization tasks in parallel using UE TaskGraph
 */
class TAPCORE_API FTapInitTaskExecutor
{
public:
	/**
	 * Execute all registered initialization tasks
	 * @param Config TapSDK configuration
	 * @return true if all tasks executed successfully
	 */
	static bool ExecuteAll(const FTapTapSdkOptions& Config);

private:
	/**
	 * Execute a single task with dependency management
	 * @param Node Task node to execute
	 * @param Config TapSDK configuration
	 * @return Completion event
	 */
	static FGraphEventRef ExecuteTask(FTapInitTaskNode* Node, const FTapTapSdkOptions& Config);

	/**
	 * Filter tasks based on build configuration
	 * @param Tasks Input tasks
	 * @param OutFilteredTasks Output filtered tasks
	 */
	static void FilterTasks(const TMap<FName, ITapInitTask*>& Tasks,
		TMap<FName, ITapInitTask*>& OutFilteredTasks);
};

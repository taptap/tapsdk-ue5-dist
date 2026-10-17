// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internal/Init/TapInitTask.h"
#include "Async/TaskGraphInterfaces.h"

/**
 * Initialization Task Node in DAG
 */
struct TAPCORE_API FTapInitTaskNode
{
	ITapInitTask* Task = nullptr;
	TArray<FTapInitTaskNode*> Dependencies;   // Tasks this task depends on
	TArray<FTapInitTaskNode*> Dependents;     // Tasks that depend on this task
	int32 InDegree = 0;                        // In-degree for topological sort
	FGraphEventRef CompletionEvent;            // Completion event for async execution

	explicit FTapInitTaskNode(ITapInitTask* InTask) : Task(InTask) {}
};

/**
 * Initialization Task DAG (Directed Acyclic Graph)
 * Manages task dependencies and provides topological sorting
 */
class TAPCORE_API FTapInitTaskGraph
{
public:
	FTapInitTaskGraph() = default;
	~FTapInitTaskGraph();

	/**
	 * Build DAG from registered tasks
	 * @return true if DAG is built successfully, false if there are missing dependencies
	 */
	bool BuildDAG(const TMap<FName, ITapInitTask*>& Tasks);

	/**
	 * Topological sort using Kahn's algorithm
	 * @param OutSortedNodes Output sorted nodes
	 * @return true if sort is successful, false if circular dependency detected
	 */
	bool TopologicalSort(TArray<FTapInitTaskNode*>& OutSortedNodes);

	/**
	 * Stable sort by priority (maintains topological order)
	 * @param SortedNodes Nodes to sort
	 */
	void StableSortByPriority(TArray<FTapInitTaskNode*>& SortedNodes);

	/** Get all nodes */
	const TMap<FName, TSharedPtr<FTapInitTaskNode>>& GetNodes() const { return Nodes; }

private:
	/**
	 * Detect circular dependency using DFS
	 * @param CyclePath Output cycle path if found
	 * @return true if cycle detected
	 */
	bool DetectCycle(TArray<FTapInitTaskNode*>& CyclePath);

	/**
	 * DFS helper for cycle detection
	 */
	bool DFS(FTapInitTaskNode* Node,
		TSet<FTapInitTaskNode*>& Visited,
		TSet<FTapInitTaskNode*>& RecStack,
		TArray<FTapInitTaskNode*>& Path);

	TMap<FName, TSharedPtr<FTapInitTaskNode>> Nodes;
};

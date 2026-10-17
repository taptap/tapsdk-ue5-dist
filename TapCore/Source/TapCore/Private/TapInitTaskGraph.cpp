// Copyright TapTap. All Rights Reserved.

#include "Init/TapInitTaskGraph.h"

FTapInitTaskGraph::~FTapInitTaskGraph()
{
	Nodes.Empty();
}

bool FTapInitTaskGraph::BuildDAG(const TMap<FName, ITapInitTask*>& Tasks)
{
	// 1. Create all nodes
	for (const auto& Pair : Tasks)
	{
		Nodes.Add(Pair.Key, MakeShared<FTapInitTaskNode>(Pair.Value));
	}

	// 2. Build dependency relationships
	bool bSuccess = true;
	for (const auto& Pair : Nodes)
	{
		FTapInitTaskNode* Node = Pair.Value.Get();
		TArray<FName> Dependencies = Node->Task->GetDependencies();

		for (const FName& DepID : Dependencies)
		{
			TSharedPtr<FTapInitTaskNode>* DepNodePtr = Nodes.Find(DepID);
			if (!DepNodePtr)
			{
				UE_LOG(LogTapCore, Error,
					TEXT("Task '%s' depends on non-existent task '%s'"),
					*Pair.Key.ToString(), *DepID.ToString());
				bSuccess = false;
				continue;
			}

			FTapInitTaskNode* DepNode = DepNodePtr->Get();
			Node->Dependencies.Add(DepNode);
			DepNode->Dependents.Add(Node);
			Node->InDegree++;
		}
	}

	return bSuccess;
}

bool FTapInitTaskGraph::TopologicalSort(TArray<FTapInitTaskNode*>& OutSortedNodes)
{
	TQueue<FTapInitTaskNode*> ZeroInDegreeQueue;
	TMap<FTapInitTaskNode*, int32> InDegreeMap;

	// Initialize in-degrees
	for (const auto& Pair : Nodes)
	{
		FTapInitTaskNode* Node = Pair.Value.Get();
		InDegreeMap.Add(Node, Node->InDegree);

		if (Node->InDegree == 0)
		{
			ZeroInDegreeQueue.Enqueue(Node);
		}
	}

	// Kahn's algorithm
	while (!ZeroInDegreeQueue.IsEmpty())
	{
		FTapInitTaskNode* Node;
		ZeroInDegreeQueue.Dequeue(Node);
		OutSortedNodes.Add(Node);

		for (FTapInitTaskNode* Dependent : Node->Dependents)
		{
			int32& InDegree = InDegreeMap[Dependent];
			InDegree--;

			if (InDegree == 0)
			{
				ZeroInDegreeQueue.Enqueue(Dependent);
			}
		}
	}

	// Check for circular dependency
	if (OutSortedNodes.Num() != Nodes.Num())
	{
		UE_LOG(LogTapCore, Error, TEXT("Circular dependency detected!"));

		TArray<FTapInitTaskNode*> CyclePath;
		if (DetectCycle(CyclePath))
		{
			FString CycleStr;
			for (int32 i = 0; i < CyclePath.Num(); i++)
			{
				CycleStr += CyclePath[i]->Task->GetTaskID().ToString();
				if (i < CyclePath.Num() - 1)
				{
					CycleStr += TEXT(" -> ");
				}
			}
			UE_LOG(LogTapCore, Error, TEXT("Cycle path: %s"), *CycleStr);
		}

		return false;
	}

	return true;
}

void FTapInitTaskGraph::StableSortByPriority(TArray<FTapInitTaskNode*>& SortedNodes)
{
	SortedNodes.StableSort([](const FTapInitTaskNode& A, const FTapInitTaskNode& B)
	{
		return A.Task->GetPriority() < B.Task->GetPriority();
	});
}

bool FTapInitTaskGraph::DetectCycle(TArray<FTapInitTaskNode*>& CyclePath)
{
	TSet<FTapInitTaskNode*> Visited;
	TSet<FTapInitTaskNode*> RecStack;

	for (const auto& Pair : Nodes)
	{
		if (DFS(Pair.Value.Get(), Visited, RecStack, CyclePath))
		{
			return true;
		}
	}

	return false;
}

bool FTapInitTaskGraph::DFS(FTapInitTaskNode* Node,
	TSet<FTapInitTaskNode*>& Visited,
	TSet<FTapInitTaskNode*>& RecStack,
	TArray<FTapInitTaskNode*>& Path)
{
	if (RecStack.Contains(Node))
	{
		Path.Add(Node);
		return true;
	}

	if (Visited.Contains(Node))
	{
		return false;
	}

	Visited.Add(Node);
	RecStack.Add(Node);
	Path.Add(Node);

	for (FTapInitTaskNode* Dep : Node->Dependencies)
	{
		if (DFS(Dep, Visited, RecStack, Path))
		{
			return true;
		}
	}

	RecStack.Remove(Node);
	Path.Pop();
	return false;
}

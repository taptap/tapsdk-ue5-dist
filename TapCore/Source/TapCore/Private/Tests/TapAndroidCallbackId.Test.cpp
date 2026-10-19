// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Internal/TapAndroidCallbackId.h"
#include "Async/ParallelFor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapAndroidCallbackIdUniqueTest,
	"TapSDK.Core.AndroidCallbacks.ConcurrentUniqueIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FTapAndroidCallbackIdUniqueTest::RunTest(const FString&)
{
	// Persistent listeners remain registered while other modules allocate one-shot calls.
	const int64 PersistentId = FTapAndroidCallbackId::Allocate();
	TArray<int64> Ids;
	Ids.SetNumUninitialized(100001); // Cross the old per-module range boundary.
	ParallelFor(Ids.Num(), [&](int32 Index)
	{
		Ids[Index] = FTapAndroidCallbackId::Allocate();
	});
	TSet<int64> UniqueIds;
	UniqueIds.Add(PersistentId);
	bool bAllPositive = PersistentId > 0;
	for (int64 Id : Ids)
	{
		bAllPositive &= Id > 0;
		UniqueIds.Add(Id);
	}
	TestTrue(TEXT("Zero remains reserved for an absent callback"), bAllPositive);
	TestEqual(TEXT("Concurrent one-shot calls cannot collide with each other or a persistent listener"),
		UniqueIds.Num(), Ids.Num() + 1);
	TestFalse(TEXT("A replacement listener cannot reuse any prior callback ID"),
		UniqueIds.Contains(FTapAndroidCallbackId::Allocate()));
	return true;
}
#endif

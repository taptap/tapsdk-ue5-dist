// Copyright TapTap. All Rights Reserved.
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Mobile/TapComplianceAsyncRequests.h"
#include "Async/ParallelFor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComplianceImmediateResponseTest,
	"TapCompliance.AndroidBridge.ImmediateResponse",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FComplianceImmediateResponseTest::RunTest(const FString&)
{
	for (const FString& Response : {FString(), FString(TEXT("invalid")), FString(TEXT("{}")),
		FString(TEXT("{\"success\":\"true\"}")),
		FString(TEXT("{\"success\":false,\"message\":\"Activity is destroyed or finishing\"}")),
		FString(TEXT("{\"success\":false,\"async\":true,\"message\":\"failed\"}"))})
	{
		FTapComplianceAsyncRequests Requests;
		int32 Calls = 0, Released = 0;
		FString Received;
		const int64 Id = Requests.Add([&](const FString& Value) { ++Calls; Received = Value; });
		Requests.AttachCleanup(Id, [&]() { ++Released; });
		Requests.HandleImmediateResponse(Id, Response);
		TestEqual(TEXT("Immediate failure completes once"), Calls, 1);
		TestEqual(TEXT("Immediate failure releases reference"), Released, 1);
		TSharedPtr<FJsonObject> Error;
		auto Reader = TJsonReaderFactory<>::Create(Received);
		TestTrue(TEXT("Failure is valid JSON"), FJsonSerializer::Deserialize(Reader, Error));
		TestTrue(TEXT("Failure remains in error channel"), Error.IsValid() && !Error->GetBoolField(TEXT("success")));
		if (Response.Contains(TEXT("Activity"))) TestEqual(TEXT("Native error is preserved"), Received, Response);
		TestFalse(TEXT("Late callback cannot complete again"), Requests.Complete(Id, TEXT("late")));
		TestEqual(TEXT("Late callback cannot release twice"), Released, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComplianceAsyncCompletionTest,
	"TapCompliance.AndroidBridge.CompletionOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FComplianceAsyncCompletionTest::RunTest(const FString&)
{
	FTapComplianceAsyncRequests Requests;
	int32 Calls = 0, Released = 0;
	FString Received;
	const int64 Id = Requests.Add([&](const FString& Value) { ++Calls; Received = Value; });
	Requests.AttachCleanup(Id, [&]() { ++Released; });
	Requests.HandleImmediateResponse(Id, TEXT("{\"success\":true,\"async\":true}"));
	TestEqual(TEXT("Async acknowledgement keeps callback pending"), Calls, 0);
	TestEqual(TEXT("Async acknowledgement retains JNI reference"), Released, 0);
	ParallelFor(32, [&](int32) { Requests.Complete(Id, TEXT("native result")); });
	TestEqual(TEXT("Concurrent callbacks complete once"), Calls, 1);
	TestEqual(TEXT("Concurrent callbacks release once"), Released, 1);
	TestEqual(TEXT("Original result is preserved"), Received, FString(TEXT("native result")));

	// Java may deliver a callback before its route invocation returns or throws.
	Requests.HandleImmediateResponse(Id, TEXT("{\"success\":false,\"message\":\"late error\"}"));
	Requests.Fail(Id, TEXT("JNI exception"));
	TestEqual(TEXT("Return and exception after completion do not redeliver"), Calls, 1);
	TestEqual(TEXT("Completed JNI reference is not freed again"), Released, 1);

	const int64 ErrorId = Requests.Add([&](const FString& Value) { ++Calls; Received = Value; });
	Requests.AttachCleanup(ErrorId, [&]() { ++Released; });
	Requests.Fail(ErrorId, TEXT("JNI exception: \"quoted\"\nmessage"));
	TestEqual(TEXT("JNI failure completes request"), Calls, 2);
	TestEqual(TEXT("JNI failure releases reference"), Released, 2);
	TSharedPtr<FJsonObject> Error;
	auto Reader = TJsonReaderFactory<>::Create(Received);
	TestTrue(TEXT("Error message is escaped"), FJsonSerializer::Deserialize(Reader, Error));
	TestFalse(TEXT("JNI failure removes pending request"), Requests.Complete(ErrorId, TEXT("late")));

	// Early allocation failures happen before there is a global reference to release.
	const int64 EarlyId = Requests.Add([&](const FString&) { ++Calls; });
	Requests.Fail(EarlyId, TEXT("Failed to create callback object"));
	Requests.AttachCleanup(EarlyId, [&]() { ++Released; });
	TestEqual(TEXT("Early failure completes once"), Calls, 3);
	TestEqual(TEXT("Late-attached reference is released"), Released, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComplianceCallbackIdIsolationTest,
	"TapCompliance.AndroidBridge.GlobalCallbackIdIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)

bool FComplianceCallbackIdIsolationTest::RunTest(const FString&)
{
	FTapComplianceAsyncRequests FirstRegistry;
	FTapComplianceAsyncRequests SecondRegistry;
	int32 FirstCalls = 0, SecondCalls = 0;
	const int64 FirstId = FirstRegistry.Add([&](const FString&) { ++FirstCalls; });
	const int64 CoreId = FTapAndroidCallbackId::Allocate();
	const int64 SecondId = SecondRegistry.Add([&](const FString&) { ++SecondCalls; });

	TestTrue(TEXT("Separate registries and Core share one ID namespace"),
		FirstId != SecondId && FirstId != CoreId && SecondId != CoreId);
	TestFalse(TEXT("First handler must not consume the second registry's result"),
		FirstRegistry.Complete(SecondId, TEXT("second")));
	TestFalse(TEXT("Registry must not consume another module's result"),
		SecondRegistry.Complete(CoreId, TEXT("core")));
	TestTrue(TEXT("Second handler receives its own result"), SecondRegistry.Complete(SecondId, TEXT("second")));
	TestTrue(TEXT("First handler still receives its own result"), FirstRegistry.Complete(FirstId, TEXT("first")));
	TestEqual(TEXT("First callback completes exactly once"), FirstCalls, 1);
	TestEqual(TEXT("Second callback completes exactly once"), SecondCalls, 1);
	return true;
}
#endif

// Copyright TapTap. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "TapBattle.h"
#include "TapOnlineBattleCallbackManager.h"
#include "Platform/ITapBattlePlatform.h"
#include "Internal/TapSdkChecker.h"
#include "Internal/Http/TapHttp.h"
#include "Tests/TapOnlineBattleCallbackManagerTestTypes.h"

namespace
{
	class FFakeTapBattlePlatform : public ITapBattlePlatform
	{
	public:
		bool bKickCalled = false;
		FString LastPlayerId;
		int64 GeneratorHandle = 42;
		int32 LastSeed = 0;
		int32 RandomCalls = 0;
		int32 FreeCalls = 0;

		virtual void Initialize() override {}
		virtual bool IsInitialized() const override { return true; }
		virtual FString GetPlatformName() const override { return TEXT("Fake"); }
		virtual void SetLogLevel(int32, bool) override {}
		virtual FString GetNativeVersion() override { return TEXT("test"); }
		virtual int64 NewRandomNumberGenerator(int32 Seed) override { LastSeed = Seed; return GeneratorHandle; }
		virtual int32 RandomInt(int64) override { ++RandomCalls; return 123; }
		virtual void FreeRandomNumberGenerator(int64) override { ++FreeCalls; }
		virtual void Connect(const FTapOnBattlePlayerIdResult&, const FTapOnBattleError&) override {}
		virtual void Disconnect(const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void CreateRoom(const FTapBattleCreateRoomConfig&, const FTapOnBattleRoomResult&, const FTapOnBattleError&) override {}
		virtual void MatchRoom(const FTapBattleMatchRoomConfig&, const FTapOnBattleRoomResult&, const FTapOnBattleError&) override {}
		virtual void GetRoomList(const FString&, int32, int32, const FTapOnBattleRoomListResult&, const FTapOnBattleError&) override {}
		virtual void JoinRoom(const FString&, const FTapBattlePlayerConfig&, const FTapOnBattleRoomResult&, const FTapOnBattleError&) override {}
		virtual void LeaveRoom(const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void UpdatePlayerCustomStatus(int32, const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void UpdatePlayerCustomProperties(const FString&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void UpdateRoomProperties(const FString&, const FString&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void KickRoomPlayer(const FString& PlayerId, const FTapOnBattleSuccess&, const FTapOnBattleError& OnError) override
		{
			bKickCalled = true;
			LastPlayerId = PlayerId;
			OnError.ExecuteIfBound(TapBattleResultCode::ERROR_INVALID_PARAMS, TEXT("invalid parameters"));
		}
		virtual void StartFrameSync(const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void SendFrameInput(const FString&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void StopFrameSync(const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
		virtual void SendCustomMessage(const FString&, int32, const TArray<FString>&, const FTapOnBattleSuccess&, const FTapOnBattleError&) override {}
	};

	void SetSdkInitializedForBattleFacadeTest(bool bInitialized)
	{
		if (bInitialized && !FTapHttp::IsInitialized())
		{
			FTapHttp::Init(TEXT("test_client"), TEXT("test_token"), ETapRegion::CN, false);
		}
		FTapSdkChecker::SetInitializationState(bInitialized, false);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapBattleKickRoomPlayerPassthroughTest,
	"TapSDK.TapBattle.UTapBattle.KickRoomPlayerInvalidParamPassthrough",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapBattleKickRoomPlayerPassthroughTest::RunTest(const FString& Parameters)
{
	SetSdkInitializedForBattleFacadeTest(true);
	const TSharedRef<FFakeTapBattlePlatform> Fake = MakeShared<FFakeTapBattlePlatform>();
	FTapBattlePlatformFactory::SetPlatformForTest(Fake);

	int32 GotCode = 0;
	FString GotMessage;
	UTapBattle::KickRoomPlayer(
		TEXT(""),
		FTapOnBattleSuccess(),
		FTapOnBattleError::CreateLambda([&](int32 Code, const FString& Message)
		{
			GotCode = Code;
			GotMessage = Message;
		}));

	TestTrue(TEXT("Empty player id reaches the native platform"), Fake->bKickCalled);
	TestEqual(TEXT("Empty player id is forwarded unchanged"), Fake->LastPlayerId, FString());
	TestEqual(TEXT("Native invalid-parameter code is preserved"), GotCode, TapBattleResultCode::ERROR_INVALID_PARAMS);
	TestEqual(TEXT("Native invalid-parameter message is preserved"), GotMessage, FString(TEXT("invalid parameters")));

	FTapBattlePlatformFactory::SetPlatformForTest(nullptr);
	SetSdkInitializedForBattleFacadeTest(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FTapOnlineBattleCallbackManagerTest,
	"TapSDK.TapBattle.CallbackManager.RegisterDispatchUnregister",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapOnlineBattleCallbackManagerTest::RunTest(const FString& Parameters)
{
	FTapOnlineBattleCallbackManager& Manager = FTapOnlineBattleCallbackManager::Get();
	Manager.Reset();

	UTapOnlineBattleCallbackManagerTestReceiver* Receiver = NewObject<UTapOnlineBattleCallbackManagerTestReceiver>();
	Receiver->AddToRoot();
	TScriptInterface<ITapOnlineBattleCallback> Callback;
	Callback.SetObject(Receiver);
	Callback.SetInterface(Cast<ITapOnlineBattleCallback>(Receiver));

	Manager.Register(Callback);
	Manager.Register(Callback);
	TestEqual(TEXT("Register deduplicates the same UObject"), Manager.GetRegisteredCallbackCountForTest(), 1);

	Manager.DispatchDisconnected(101, TEXT("offline"));
	TestEqual(TEXT("Disconnected dispatched once"), Receiver->DisconnectedCount, 1);
	TestEqual(TEXT("Disconnected code preserved"), Receiver->LastCode, 101);
	TestEqual(TEXT("Disconnected message preserved"), Receiver->LastMessage, FString(TEXT("offline")));

	FTapBattleFrameSyncInfo FrameSyncInfo;
	FrameSyncInfo.Seed = 42;
	Manager.DispatchFrameSyncStarted(FrameSyncInfo);
	TestEqual(TEXT("Frame sync event dispatched"), Receiver->FrameSyncStartedCount, 1);
	TestEqual(TEXT("Frame sync seed preserved"), Receiver->LastSeed, 42);

	Manager.Unregister(Callback);
	TestEqual(TEXT("Unregister removes callback"), Manager.GetRegisteredCallbackCountForTest(), 0);
	Manager.DispatchDisconnected(7, TEXT("ignored"));
	TestEqual(TEXT("Unregistered callback is not invoked"), Receiver->DisconnectedCount, 1);

	Receiver->RemoveFromRoot();
	Manager.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTapBattleRandomLifetimeTest,
	"TapSDK.TapBattle.RandomGenerator.Ownership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTapBattleRandomLifetimeTest::RunTest(const FString&)
{
	SetSdkInitializedForBattleFacadeTest(true);
	auto Fake = MakeShared<FFakeTapBattlePlatform>();
	FTapBattlePlatformFactory::SetPlatformForTest(Fake);
	{
		auto Generator = UTapBattle::NewRandomNumberGenerator(7);
		if (!TestTrue(TEXT("Generator created"), Generator.IsValid())) return false;
		TestEqual(TEXT("Seed forwarded"), Fake->LastSeed, 7);
		TestEqual(TEXT("Random value forwarded"), Generator->RandomInt(), 123);
		// Free must use the creator, even if initialization or the factory has changed.
		FTapBattlePlatformFactory::SetPlatformForTest(MakeShared<FFakeTapBattlePlatform>());
		SetSdkInitializedForBattleFacadeTest(false);
		Generator->Free();
		Generator->Free();
		TestEqual(TEXT("Free is idempotent"), Fake->FreeCalls, 1);
		TestEqual(TEXT("Released generator returns zero"), Generator->RandomInt(), 0);
		TestEqual(TEXT("No native call after release"), Fake->RandomCalls, 1);
	}
	TestEqual(TEXT("Destructor does not double free"), Fake->FreeCalls, 1);
	SetSdkInitializedForBattleFacadeTest(true);
	FTapBattlePlatformFactory::SetPlatformForTest(Fake);
	{
		auto Generator = UTapBattle::NewRandomNumberGenerator(8);
		auto Moved = MoveTemp(Generator);
		TestFalse(TEXT("Unique ownership transfers"), Generator.IsValid());
	}
	TestEqual(TEXT("Destruction releases native resource"), Fake->FreeCalls, 2);
	for (int64 Invalid : {int64(0), int64(-1)})
	{
		Fake->GeneratorHandle = Invalid;
		TestFalse(TEXT("Failed creation returns null"), UTapBattle::NewRandomNumberGenerator(9).IsValid());
	}
	TestEqual(TEXT("Invalid handles are not freed"), Fake->FreeCalls, 2);
	FTapBattlePlatformFactory::SetPlatformForTest(nullptr);
	SetSdkInitializedForBattleFacadeTest(false);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapOnlineBattleCallback.h"
#include "TapOnlineBattleCallbackManagerTestTypes.generated.h"

/** Private reflected receiver used by callback manager automation tests. */
UCLASS()
class UTapOnlineBattleCallbackManagerTestReceiver : public UObject, public ITapOnlineBattleCallback
{
	GENERATED_BODY()

public:
	virtual void OnDisconnected_Implementation(int32 Code, const FString& Message) override
	{
		++DisconnectedCount;
		LastCode = Code;
		LastMessage = Message;
	}

	virtual void OnFrameSyncStarted_Implementation(const FTapBattleFrameSyncInfo& FrameSyncInfo) override
	{
		++FrameSyncStartedCount;
		LastSeed = FrameSyncInfo.Seed;
	}

	int32 DisconnectedCount = 0;
	int32 FrameSyncStartedCount = 0;
	int32 LastCode = 0;
	int32 LastSeed = 0;
	FString LastMessage;
};

#pragma once
#include "CoreMinimal.h"

class ITapBattlePlatform;

/** Owns a deterministic native generator. Destruction or Free releases it once. */
class TAPBATTLE_API FTapBattleRandomNumberGenerator
{
public:
	~FTapBattleRandomNumberGenerator();
	FTapBattleRandomNumberGenerator(const FTapBattleRandomNumberGenerator&) = delete;
	FTapBattleRandomNumberGenerator& operator=(const FTapBattleRandomNumberGenerator&) = delete;
	int32 RandomInt();
	void Free();

private:
	friend class UTapBattle;
	FTapBattleRandomNumberGenerator(TSharedPtr<ITapBattlePlatform> InPlatform, int64 InHandle);
	TSharedPtr<ITapBattlePlatform> Platform;
	int64 Handle;
};

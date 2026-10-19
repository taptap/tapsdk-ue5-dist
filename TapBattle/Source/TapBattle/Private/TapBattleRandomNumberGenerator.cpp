#include "TapBattleRandomNumberGenerator.h"
#include "Platform/ITapBattlePlatform.h"

FTapBattleRandomNumberGenerator::FTapBattleRandomNumberGenerator(TSharedPtr<ITapBattlePlatform> InPlatform, int64 InHandle)
	: Platform(MoveTemp(InPlatform)), Handle(InHandle) {}

FTapBattleRandomNumberGenerator::~FTapBattleRandomNumberGenerator()
{
	Free();
}

int32 FTapBattleRandomNumberGenerator::RandomInt()
{
	return Platform.IsValid() ? Platform->RandomInt(Handle) : 0;
}

void FTapBattleRandomNumberGenerator::Free()
{
	if (Platform.IsValid())
	{
		Platform->FreeRandomNumberGenerator(Handle);
		Platform.Reset();
		Handle = 0;
	}
}

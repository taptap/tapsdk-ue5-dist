#pragma once
#include "CoreMinimal.h"

enum class ETapCloudSaveState : uint8
{
	Unknown   = 0,
	NeedLogin = 1,
	InitFail  = 2,
};

DECLARE_DELEGATE_TwoParams(FTapOnCloudSaveState,     ETapCloudSaveState /*State*/, int32 /*RawCode*/);

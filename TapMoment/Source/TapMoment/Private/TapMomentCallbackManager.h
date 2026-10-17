#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapMomentCallback.h"
class FTapMomentCallbackManager{public:static FTapMomentCallbackManager&Get();void Set(UObject*O){R.Reset();R.Register(O,UTapMomentCallback::StaticClass());}void Reset(){R.Reset();}void Dispatch(int32 C,const FString&M);private:FTapWeakCallbackRegistry R;};

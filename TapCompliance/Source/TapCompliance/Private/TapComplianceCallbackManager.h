#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapCompliance.h"
class FTapComplianceCallbackManager{public:static FTapComplianceCallbackManager&Get();void Register(UObject*O){R.Register(O,UTapComplianceCallback::StaticClass());}void Unregister(UObject*O){R.Unregister(O);}int32 Num(){return R.Num();}void Reset(){R.Reset();}void Dispatch(int32 C,const FString&E);private:FTapWeakCallbackRegistry R;};

#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapCloudSaveCallback.h"
#include "TapCloudSaveTypes.h"
#include "TapCloudSaveInternalTypes.h"
class FTapCloudSaveCallbackManager{public:static FTapCloudSaveCallbackManager&Get();void Register(UObject*O){R.Register(O,UTapCloudSaveCallback::StaticClass());}void Unregister(UObject*O){R.Unregister(O);}int32 Num(){return R.Num();}void Reset(){R.Reset();}void Dispatch(ETapCloudSaveState,int32 Raw);const FTapOnCloudSaveState& NativeDelegate();private:FTapWeakCallbackRegistry R;FTapOnCloudSaveState Native;};

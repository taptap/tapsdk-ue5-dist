#include "TapCloudSaveCallbackManager.h"
FTapCloudSaveCallbackManager&FTapCloudSaveCallbackManager::Get(){static FTapCloudSaveCallbackManager I;return I;}
void FTapCloudSaveCallbackManager::Dispatch(ETapCloudSaveState,int32 C){R.Dispatch([C](UObject*O){ITapCloudSaveCallback::Execute_OnResult(O,C);});}
const FTapOnCloudSaveState& FTapCloudSaveCallbackManager::NativeDelegate(){if(!Native.IsBound())Native.BindRaw(this,&FTapCloudSaveCallbackManager::Dispatch);return Native;}

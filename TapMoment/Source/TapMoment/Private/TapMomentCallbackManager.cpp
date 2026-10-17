#include "TapMomentCallbackManager.h"
FTapMomentCallbackManager&FTapMomentCallbackManager::Get(){static FTapMomentCallbackManager I;return I;}void FTapMomentCallbackManager::Dispatch(int32 C,const FString&M){R.Dispatch([C,M](UObject*O){ITapMomentCallback::Execute_OnMomentResult(O,C,M);});}

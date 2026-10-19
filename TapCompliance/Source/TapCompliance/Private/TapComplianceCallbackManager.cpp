#include "TapComplianceCallbackManager.h"
FTapComplianceCallbackManager&FTapComplianceCallbackManager::Get(){static FTapComplianceCallbackManager I;return I;}void FTapComplianceCallbackManager::Dispatch(int32 C,const FString&E){R.Dispatch([C,E](UObject*O){ITapComplianceCallback::Execute_OnComplianceResult(O,C,E);});}

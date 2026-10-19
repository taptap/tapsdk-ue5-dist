#include "TapAchievementCallbackManager.h"
FTapAchievementCallbackManager&FTapAchievementCallbackManager::Get(){static FTapAchievementCallbackManager I;return I;}
void FTapAchievementCallbackManager::Success(int32 C,const FTapAchievementResult&V){R.Dispatch([C,V](UObject*O){ITapAchievementCallback::Execute_OnAchievementSuccess(O,C,V);});
#if WITH_DEV_AUTOMATION_TESTS
TestSuccess.Broadcast(C,V);
#endif
}
void FTapAchievementCallbackManager::Failure(const FString&I,int32 C,const FString&M){R.Dispatch([I,C,M](UObject*O){ITapAchievementCallback::Execute_OnAchievementFailure(O,I,C,M);});
#if WITH_DEV_AUTOMATION_TESTS
TestFailure.Broadcast(I,C,M);
#endif
}

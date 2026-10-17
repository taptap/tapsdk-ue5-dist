#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapAchievementCallback.h"
class FTapAchievementCallbackManager{public:static FTapAchievementCallbackManager&Get();void Register(UObject*O){R.Register(O,UTapAchievementCallback::StaticClass());}void Unregister(UObject*O){R.Unregister(O);}void Reset(){R.Reset();}void Success(int32 C,const FTapAchievementResult&V);void Failure(const FString&I,int32 C,const FString&M);
#if WITH_DEV_AUTOMATION_TESTS
 FOnAchievementSuccess TestSuccess;FOnAchievementFailure TestFailure;
#endif
private:FTapWeakCallbackRegistry R;};

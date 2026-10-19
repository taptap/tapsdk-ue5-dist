#include "TapLicenseCallbackManager.h"
FTapLicenseCallbackManager&FTapLicenseCallbackManager::Get(){static FTapLicenseCallbackManager I;return I;}void FTapLicenseCallbackManager::Success(){License.Dispatch([](UObject*O){ITapLicenseCallback::Execute_OnLicenseSuccess(O);});
#if WITH_DEV_AUTOMATION_TESTS
TestSuccess.Broadcast();
#endif
}void FTapLicenseCallbackManager::Failed(){License.Dispatch([](UObject*O){ITapLicenseCallback::Execute_OnLicenseFailed(O);});
#if WITH_DEV_AUTOMATION_TESTS
TestFailed.Broadcast();
#endif
}void FTapLicenseCallbackManager::Query(ETapLicenseQueryCode C,const FTapLicenseDLCResultMap&R){DLC.Dispatch([C,R](UObject*O){ITapDLCCallback::Execute_OnQueryCallback(O,C,R);});
#if WITH_DEV_AUTOMATION_TESTS
TestQuery.Broadcast(C,R);
#endif
}void FTapLicenseCallbackManager::Order(const FString&S,ETapDLCPurchasedCode C){DLC.Dispatch([S,C](UObject*O){ITapDLCCallback::Execute_OnOrderCallback(O,S,C);});
#if WITH_DEV_AUTOMATION_TESTS
TestOrder.Broadcast(S,C);
#endif
}

#pragma once
#include "Internal/TapWeakCallbackRegistry.h"
#include "TapLicenseCallback.h"
class FTapLicenseCallbackManager{public:static FTapLicenseCallbackManager&Get();FTapWeakCallbackRegistry License,DLC;void Reset(){License.Reset();DLC.Reset();}void Success();void Failed();void Query(ETapLicenseQueryCode,const FTapLicenseDLCResultMap&);void Order(const FString&,ETapDLCPurchasedCode);
#if WITH_DEV_AUTOMATION_TESTS
FOnLicenseSuccess TestSuccess;FOnLicenseFailed TestFailed;FOnLicenseError TestError;FOnDLCQueryResult TestQuery;FOnDLCOrderChanged TestOrder;
#endif
};

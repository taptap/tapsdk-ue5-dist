// Copyright TapTap. All Rights Reserved.
#include "TapLicenseiOS.h"

#if PLATFORM_IOS

#import "TapLicenseIOSBridge.h"
#include "Async/Async.h"
#include "Containers/Map.h"

void FTapLicenseiOS::SetupCallbacks(
	FLicenseSuccessFn OnSuccess, FLicenseFailedFn OnFailed,
	FDLCQueryFn OnDLCQuery, FDLCOrderFn OnDLCOrder)
{
	CbSuccess  = MoveTemp(OnSuccess);
	CbFailed   = MoveTemp(OnFailed);
	CbDLCQuery = MoveTemp(OnDLCQuery);
	CbDLCOrder = MoveTemp(OnDLCOrder);
}

void FTapLicenseiOS::Initialize()
{
	FLicenseSuccessFn Success = CbSuccess;
	FLicenseFailedFn  Failed  = CbFailed;
	FDLCOrderFn       Order   = CbDLCOrder;

	[TapLicenseIOSBridge
		setupWithResultBlock:^(BOOL owned)
		{
			AsyncTask(ENamedThreads::GameThread, [Success, Failed, owned]()
			{
				if (owned) { if (Success) Success(); }
				else       { if (Failed)  Failed();  }
			});
		}
		orderBlock:^(NSString* sku, NSInteger status)
		{
			FString Sku = FString(sku);
			const ETapDLCPurchasedCode Code = TapLicenseDLCStatus::FromNativeValue(static_cast<int32>(status));
			AsyncTask(ENamedThreads::GameThread, [Order, Sku, Code]()
			{
				if (Order) Order(Sku, Code);
			});
		}];
}

void FTapLicenseiOS::Shutdown() {}

void FTapLicenseiOS::CheckLicense(bool bForce)
{
	[TapLicenseIOSBridge checkLicense:(bForce ? YES : NO)];
}

void FTapLicenseiOS::QueryDLC(const TArray<FString>& DLCList)
{
	NSMutableArray<NSString*>* Skus = [NSMutableArray arrayWithCapacity:DLCList.Num()];
	for (const FString& S : DLCList) { [Skus addObject:S.GetNSString()]; }

	FDLCQueryFn Query = CbDLCQuery;
	[TapLicenseIOSBridge queryDLC:Skus resultBlock:^(NSInteger code, NSDictionary<NSString*, NSNumber*>* results)
	{
		ETapLicenseQueryCode Code = static_cast<ETapLicenseQueryCode>((uint8)code);
		FTapLicenseDLCResultMap Map;
		for (NSString* Key in results) { Map.Add(FString(Key), [results[Key] boolValue]); }
		AsyncTask(ENamedThreads::GameThread, [Query, Code, Map]()
		{
			if (Query) Query(Code, Map);
		});
	}];
}

void FTapLicenseiOS::PurchaseDLC(const FString& DLCID)
{
	[TapLicenseIOSBridge purchaseDLC:DLCID.GetNSString()];
}

#endif // PLATFORM_IOS

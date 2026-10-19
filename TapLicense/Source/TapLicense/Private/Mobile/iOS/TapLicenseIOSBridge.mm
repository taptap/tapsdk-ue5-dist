// Copyright TapTap. All Rights Reserved.
#import "TapLicenseIOSBridge.h"
// VERIFY: 实际头文件名以交付的 xcframework 为准
// #import <TapTapLicenseSDK/TapTapLicenseSDK.h>

@implementation TapLicenseIOSBridge

+ (void)setupWithResultBlock:(TapLicenseResultBlock)resultBlock
                  orderBlock:(TapLicenseOrderBlock)orderBlock
{
	// VERIFY: 注册原生 License/DLC delegate，回调里转调 resultBlock / orderBlock。
	// 所有 block 回调内部已确保在主线程（见 .mm 调用方），此处直接调用即可。
}

+ (void)checkLicense:(BOOL)force
{
	// VERIFY: [TapTapLicense checkLicense:force];
}

+ (void)queryDLC:(NSArray<NSString*>*)skuList resultBlock:(TapLicenseQueryBlock)block
{
	// VERIFY: 调原生 queryDLC，结果转成 (code, NSDictionary<sku, @(owned)>) 调 block。
}

+ (void)purchaseDLC:(NSString*)sku
{
	// VERIFY: [TapTapLicense purchaseDLC:sku]; 打开 TapTap 商店页。
}

@end

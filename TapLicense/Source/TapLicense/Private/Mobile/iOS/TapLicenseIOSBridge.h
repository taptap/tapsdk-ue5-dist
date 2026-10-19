// Copyright TapTap. All Rights Reserved.
#import <Foundation/Foundation.h>

/** DLC 查询结果回调：code（0=OK,1=未装 TapTap,2=Error）+ skuId->owned 字典。 */
typedef void (^TapLicenseQueryBlock)(NSInteger code, NSDictionary<NSString*, NSNumber*>* _Nullable results);
/** 正版结果回调：owned。 */
typedef void (^TapLicenseResultBlock)(BOOL owned);
/** DLC 订单变化回调：skuId + status（0=未购,1=已购,2=退款异常）。 */
typedef void (^TapLicenseOrderBlock)(NSString* _Nonnull sku, NSInteger status);

/**
 * 纯 ObjC bridge：C++(FTapLicenseiOS) -> 本类 -> TapTapLicenseSDK。
 * VERIFY: 以下方法体内的 SDK 类名 / 方法名以实际 xcframework 头文件为准。
 */
@interface TapLicenseIOSBridge : NSObject
+ (void)setupWithResultBlock:(TapLicenseResultBlock _Nonnull)resultBlock
                  orderBlock:(TapLicenseOrderBlock _Nonnull)orderBlock;
+ (void)checkLicense:(BOOL)force;
+ (void)queryDLC:(NSArray<NSString*>* _Nonnull)skuList
      resultBlock:(TapLicenseQueryBlock _Nonnull)block;
+ (void)purchaseDLC:(NSString* _Nonnull)sku;
@end

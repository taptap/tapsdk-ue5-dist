// Copyright TapTap. All Rights Reserved.

#import <Foundation/Foundation.h>

/**
 * Compliance event callback block type
 * code: TapComplianceResultHandlerCode (see TapTapCompliance.h)
 *   500  = LoginSuccess (登录成功)
 *   1000 = Exited (登出成功)
 *   1001 = SwitchAccount (切换账号)
 *   1030 = PeriodRestrict (不可玩时间段)
 *   1100 = AgeLimit (适龄限制)
 *   1200 = InvalidClientOrNetworkError
 *   9002 = RealNameStop (实名过程中关闭)
 */
typedef void (^TapComplianceEventBlock)(NSInteger code, NSString* _Nullable extraJson);

/**
 * Pure Objective-C bridge for TapCompliance iOS SDK
 * Architecture:
 *   C++ (ITapCompliancePlatform)
 *     -> Objective-C++ (TapComplianceiOS.mm)
 *       -> Pure ObjC (TapComplianceIOSBridge)
 *         -> TapTapComplianceSDK iOS Framework
 */
@interface TapComplianceIOSBridge : NSObject

/**
 * 注册全局合规事件回调（持久回调）
 * 内部使用 TapTapCompliance.registerComplianceDelegate:
 * @param eventBlock 事件回调 block
 */
+ (void)registerComplianceCallback:(TapComplianceEventBlock _Nonnull)eventBlock;

/**
 * 注销全局合规事件回调
 */
+ (void)unregisterComplianceCallback;

/**
 * 设置是否显示切换账号按钮
 * iOS 通过 TapComplianceService.setFunctionConfig 在 startup 前更新配置。
 * @param show YES = 显示切换账号按钮
 */
+ (void)setShowSwitchAccount:(BOOL)show;

/**
 * 设置是否暴露 / 采集年龄段信息
 * iOS 通过 TapComplianceService.setFunctionConfig 在 startup 前更新配置。
 * @param use YES（默认）= 采集并返回真实年龄段；NO = ageRange 始终返回 -1
 */
+ (void)setUseAgeRange:(BOOL)use;

/**
 * 启动合规认证流程
 * 对应 TapTapCompliance.startup:
 * @param userId 用户 ID
 */
+ (void)startupWithUserId:(NSString* _Nonnull)userId;

/**
 * 退出合规 UI
 * 对应 TapTapCompliance.exit
 */
+ (void)exit;

/**
 * 检查支付限额（异步）
 * 对应 TapTapCompliance.checkPaymentLimit:callBack:failureHandler:
 * @param amount 支付金额（单位：分）
 * @param completion (allowed, title, description) - 允许支付时 allowed=YES，不允许时带提示
 */
+ (void)checkPaymentLimit:(NSInteger)amount
               completion:(void(^ _Nonnull)(BOOL allowed, NSString* _Nullable title, NSString* _Nullable description))completion
                  failure:(void(^ _Nonnull)(NSString* _Nullable error))failure;

/**
 * 提交支付结果（异步）
 * 对应 TapTapCompliance.submitPayment:callBack:failureHandler:
 * @param amount 实际支付金额（单位：分）
 * @param completion (success, message)
 */
+ (void)submitPayment:(NSInteger)amount
           completion:(void(^ _Nullable)(BOOL success, NSString* _Nullable message))completion;

/**
 * 获取用户年龄段（同步）
 * 对应 TapTapCompliance.getAgeRange
 * @return -1=未知, 0=<8岁, 8=8-16岁, 16=16-18岁, 18=成年人
 */
+ (NSInteger)ageRange;

/**
 * 获取用户剩余游戏时间（同步）
 * 对应 TapTapCompliance.getRemainingTime
 * @return 剩余秒数，-1=未知
 */
+ (NSInteger)remainingTime;

/**
 * 获取当前用户 AccessToken（同步）
 * 对应 TapTapCompliance.getCurrentAccessToken
 * @return AccessToken 字符串，nil 表示未登录
 */
+ (NSString* _Nullable)currentAccessToken;

@end

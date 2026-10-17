// Copyright TapTap. All Rights Reserved.

#import "TapComplianceIOSBridge.h"
#import <TargetConditionals.h>
#import <TapTapComplianceSDK/TapTapCompliance.h>
#import <TapTapComplianceSDK/TapTapComplianceSDK-Swift.h>

// =============================================================================
// Internal Delegate Adapter
// Bridges iOS SDK TapTapComplianceDelegate -> C++ block callback
// =============================================================================

@interface TapComplianceDelegateAdapter : NSObject <TapTapComplianceDelegate>
@property (nonatomic, copy) TapComplianceEventBlock eventBlock;
@end

@implementation TapComplianceDelegateAdapter

- (void)complianceCallbackWithCode:(TapComplianceResultHandlerCode)code extra:(NSString *)extra
{
    NSLog(@"[TapComplianceIOSBridge] complianceCallback: code=%ld, extra=%@", (long)code, extra);
    if (self.eventBlock) {
        self.eventBlock((NSInteger)code, extra);
    }
}

@end

// =============================================================================
// TapComplianceIOSBridge Implementation
// =============================================================================

@implementation TapComplianceIOSBridge

static TapComplianceDelegateAdapter* _delegateAdapter = nil;
static TapComplianceEventBlock _storedEventBlock = nil;
static BOOL _showSwitchAccount = NO;
static BOOL _useAgeRange = YES;

#pragma mark - Callback Registration

+ (void)registerComplianceCallback:(TapComplianceEventBlock)eventBlock
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self registerComplianceCallback:eventBlock];
        });
        return;
    }

    NSLog(@"[TapComplianceIOSBridge] registerComplianceCallback");

    _storedEventBlock = [eventBlock copy];

    if (!_delegateAdapter) {
        _delegateAdapter = [[TapComplianceDelegateAdapter alloc] init];
    }
    _delegateAdapter.eventBlock = _storedEventBlock;

    [TapTapCompliance registerComplianceDelegate:_delegateAdapter];
    NSLog(@"[TapComplianceIOSBridge] Compliance delegate registered");
}

+ (void)unregisterComplianceCallback
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self unregisterComplianceCallback];
        });
        return;
    }

    NSLog(@"[TapComplianceIOSBridge] unregisterComplianceCallback");

    _storedEventBlock = nil;
    if (_delegateAdapter) {
        _delegateAdapter.eventBlock = nil;
    }
    // 传 nil 注销 delegate
    [TapTapCompliance registerComplianceDelegate:(id<TapTapComplianceDelegate>_Nonnull)nil];
}

#pragma mark - Compliance Flow

+ (void)setShowSwitchAccount:(BOOL)show
{
    NSLog(@"[TapComplianceIOSBridge] setShowSwitchAccount: %@", show ? @"YES" : @"NO");
    _showSwitchAccount = show;
    [TapComplianceService setFunctionConfig:_showSwitchAccount :_useAgeRange];
}

+ (void)setUseAgeRange:(BOOL)use
{
    NSLog(@"[TapComplianceIOSBridge] setUseAgeRange: %@", use ? @"YES" : @"NO");
    _useAgeRange = use;
    [TapComplianceService setFunctionConfig:_showSwitchAccount :_useAgeRange];
}

+ (void)startupWithUserId:(NSString*)userId
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self startupWithUserId:userId];
        });
        return;
    }

    NSLog(@"[TapComplianceIOSBridge] startup: userId=%@", userId);
    [TapComplianceService setFunctionConfig:_showSwitchAccount :_useAgeRange];
    [TapTapCompliance startup:userId];
}

+ (void)exit
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self exit];
        });
        return;
    }

    NSLog(@"[TapComplianceIOSBridge] exit");
    [TapTapCompliance exit];
}

#pragma mark - Payment

+ (void)checkPaymentLimit:(NSInteger)amount
               completion:(void(^)(BOOL allowed, NSString* _Nullable title, NSString* _Nullable description))completion
                  failure:(void(^)(NSString* _Nullable error))failure
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self checkPaymentLimit:amount completion:completion failure:failure];
        });
        return;
    }

    NSLog(@"[TapComplianceIOSBridge] checkPaymentLimit: amount=%ld", (long)amount);

    [TapTapCompliance checkPaymentLimit:amount
                               callBack:^(BOOL status, NSString * _Nonnull title, NSString * _Nonnull description) {
        NSLog(@"[TapComplianceIOSBridge] checkPaymentLimit allowed: %@, title=%@", status ? @"YES" : @"NO", title);
        if (completion) {
            completion(status, title, description);
        }
    }
                         failureHandler:^(NSString * _Nonnull error) {
        NSLog(@"[TapComplianceIOSBridge] checkPaymentLimit failed: %@", error);
        if (failure) {
            failure(error);
        }
    }];
}

+ (void)submitPayment:(NSInteger)amount
           completion:(void(^)(BOOL success, NSString* _Nullable message))completion
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self submitPayment:amount completion:completion];
        });
        return;
    }

    NSLog(@"[TapComplianceIOSBridge] submitPayment: amount=%ld", (long)amount);

    [TapTapCompliance submitPayment:amount
                           callBack:^(BOOL success) {
        NSLog(@"[TapComplianceIOSBridge] submitPayment success=%@", success ? @"YES" : @"NO");
        if (completion) {
            completion(success, nil);
        }
    }
                     failureHandler:^(NSString * _Nonnull error) {
        NSLog(@"[TapComplianceIOSBridge] submitPayment failed: %@", error);
        if (completion) {
            completion(NO, error);
        }
    }];
}

#pragma mark - Sync Queries

+ (NSInteger)ageRange
{
    NSLog(@"[TapComplianceIOSBridge] ageRange");
    TapComplianceAgeLimit result = [TapTapCompliance getAgeRange];
    return (NSInteger)result;
}

+ (NSInteger)remainingTime
{
    NSLog(@"[TapComplianceIOSBridge] remainingTime");
    return [TapTapCompliance getRemainingTime];
}

+ (NSString* _Nullable)currentAccessToken
{
    NSLog(@"[TapComplianceIOSBridge] currentAccessToken");
    return [TapTapCompliance getCurrentAccessToken];
}

@end

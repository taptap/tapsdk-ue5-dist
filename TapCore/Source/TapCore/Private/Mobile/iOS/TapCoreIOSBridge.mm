// Copyright TapTap. All Rights Reserved.

#import "TapCoreIOSBridge.h"
#import <TargetConditionals.h>

// Enable iOS SDK integration with local xcframework
#define TAPTAP_SDK_AVAILABLE 1

#if defined(TAPTAP_SDK_AVAILABLE) && TAPTAP_SDK_AVAILABLE
#import <TapTapCoreSDK/TapTapSDK.h>
#import <TapTapCoreSDK/TapTapEvent.h>
#import <TapTapBasicToolsSDK/TapTapSDKHostConfiguration.h>
#define USE_REAL_SDK 1
#else
#define USE_REAL_SDK 0
#endif

#if USE_REAL_SDK
@interface TapCoreInitCallback : NSObject <TapInitCallback>
@property(nonatomic, copy) void (^resultBlock)(NSInteger code, NSString* message);
@end

@implementation TapCoreInitCallback
- (void)finish:(NSInteger)code message:(NSString*)message {
    void (^result)(NSInteger, NSString*) = [self.resultBlock copy];
    self.resultBlock = nil;
    [TapTapSDK removeInitCallback:self];
    if (result) result(code, message);
#if !__has_feature(objc_arc)
    [result release];
#endif
}
- (void)onInitSuccess {
    [self finish:0 message:nil];
}
- (void)onInitFail:(NSInteger)errorCode errorMsg:(NSString*)errorMsg {
    [self finish:errorCode message:errorMsg];
}
@end
#endif

@implementation TapCoreIOSBridge

static BOOL _isInitialized = NO;
#if USE_REAL_SDK
static TapCoreInitCallback* _initCallback = nil;
#endif

#pragma mark - Initialization

+ (void)initWithConfig:(NSString*)configJson
            completion:(void(^)(BOOL success, NSString* errorMsg))completion
           finalResult:(void(^)(NSInteger code, NSString* message))result
{
    // Ensure this is called on main thread
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self initWithConfig:configJson completion:completion finalResult:result];
        });
        return;
    }

    _isInitialized = NO;
#if USE_REAL_SDK
    if (_initCallback) {
        [TapTapSDK removeInitCallback:_initCallback];
        _initCallback.resultBlock = nil;
#if !__has_feature(objc_arc)
        [_initCallback release];
#endif
        _initCallback = nil;
    }
#endif
    NSLog(@"[TapCoreIOSBridge] Starting iOS SDK initialization");

#if USE_REAL_SDK
    // Parse JSON configuration
    NSDictionary* configDict = [self parseJson:configJson];
    if (!configDict) {
        NSString* errorMsg = @"Failed to parse config JSON";
        NSLog(@"[TapCoreIOSBridge] Error: %@", errorMsg);
        if (completion) {
            completion(NO, errorMsg);
        }
        return;
    }

    // Extract configuration values
    NSString* clientId = configDict[@"clientId"];
    NSString* clientToken = configDict[@"clientToken"];
    NSNumber* regionNum = configDict[@"region"];
    NSNumber* preferredLanguage = configDict[@"preferredLanguage"];
    NSString* gameVersion = configDict[@"gameVersion"];
    NSNumber* enableLog = configDict[@"enableLog"];
    NSNumber* enableTapDB = configDict[@"enableTapDB"];
    NSNumber* isRND = configDict[@"isRND"] ?: configDict[@"isRnd"];

    // Validate required fields
    if (!clientId || !clientToken) {
        NSString* errorMsg = @"Missing required fields: clientId or clientToken";
        NSLog(@"[TapCoreIOSBridge] Error: %@", errorMsg);
        if (completion) {
            completion(NO, errorMsg);
        }
        return;
    }

    NSLog(@"[TapCoreIOSBridge] Config - ClientID: %@, Region: %@", clientId, regionNum);
    if (isRND) {
        NSLog(@"[TapCoreIOSBridge] Config - isRND: %@", [isRND boolValue] ? @"YES" : @"NO");
    }

    // RND 环境不走 TapTapSdkOptions 字段，而是通过 HostConfiguration + Router 事件链分发给各业务模块。
    if (isRND && [isRND boolValue]) {
        SEL switchToRndSelector = NSSelectorFromString(@"switchToRnd");
        if ([TapTapSDKHostConfiguration respondsToSelector:switchToRndSelector]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
            [TapTapSDKHostConfiguration performSelector:switchToRndSelector];
#pragma clang diagnostic pop
            NSLog(@"[TapCoreIOSBridge] Switched iOS SDK host configuration to RND");
        } else {
            NSLog(@"[TapCoreIOSBridge] Warning: TapTapSDKHostConfiguration.switchToRnd is unavailable");
        }
    }

    // Initialize iOS Native SDK
    // Create TapTapSdkOptions
    TapTapSdkOptions* options = [TapTapSdkOptions new];
    options.clientId = clientId;
    options.clientToken = clientToken;

    // Set region
    NSInteger region = [regionNum integerValue];
    options.region = (region == 0) ? TapTapRegionTypeCN : TapTapRegionTypeOverseas;
    if (preferredLanguage) {
        options.preferredLanguage = (TapLanguageType)[preferredLanguage integerValue];
    }

    // Set optional fields
    if (gameVersion && gameVersion.length > 0) {
        options.gameVersion = gameVersion;
    }

    // Set enable log
    if (enableLog) {
        options.enableLog = [enableLog boolValue];
    }

    // Screen orientation
    NSNumber* screenOrientation = configDict[@"screenOrientation"];
    if (screenOrientation) {
        options.screenOrientation = [screenOrientation integerValue] == 1
            ? TapTapScreenOrientationLandscape
            : TapTapScreenOrientationPortrait;
    }

    // Parse customProperties for event options
    NSDictionary* customProperties = configDict[@"customProperties"];

    // Create TapTapEventOptions
    // Note: caid moved from TapTapSdkOptions to TapTapEventOptions in SDK 2.2.2-alpha.29+
    NSString* caid = configDict[@"caid"];
    TapTapEventOptions* eventOptions = [TapTapEventOptions new];
    eventOptions.enableTapTapEvent = enableTapDB ? [enableTapDB boolValue] : YES;

    if (eventOptions.enableTapTapEvent) {
        if (caid && caid.length > 0) {
            eventOptions.caid = caid;
        }

        if (customProperties) {
            // channel
            NSString* legacyChannel = customProperties[@"channel"];
            if (legacyChannel && legacyChannel.length > 0) {
                eventOptions.channel = legacyChannel;
            }

            // overrideBuiltInParameters
            NSString* overrideStr = customProperties[@"overrideBuiltInParameters"];
            if (overrideStr) {
                eventOptions.overrideBuiltInParameters = [overrideStr isEqualToString:@"true"];
            }

            // autoIAPEventEnabled → enableAutoIAPEvent
            NSString* autoIAPStr = customProperties[@"autoIAPEventEnabled"];
            if (autoIAPStr) {
                eventOptions.enableAutoIAPEvent = [autoIAPStr isEqualToString:@"true"];
            }

            // properties - parse JSON object from customProperties values
            // Collect all keys that are not known config keys as custom properties
            NSMutableDictionary* eventProperties = [NSMutableDictionary dictionary];
            NSDictionary* knownKeys = @{
                @"channel": @YES,
                @"overrideBuiltInParameters": @YES,
                @"disableAutoLogDeviceLogin": @YES,
                @"autoIAPEventEnabled": @YES,
                @"enableTapTapEvent": @YES
            };
            for (NSString* key in customProperties) {
                if (!knownKeys[key]) {
                    eventProperties[key] = customProperties[key];
                }
            }
            if (eventProperties.count > 0) {
                eventOptions.properties = eventProperties;
            }
        }

        NSString* channel = configDict[@"channel"];
        if (channel && channel.length > 0) {
            eventOptions.channel = channel;
        }
        NSNumber* overrideBuiltInParameters = configDict[@"overrideBuiltInParameters"];
        if (overrideBuiltInParameters) {
            eventOptions.overrideBuiltInParameters = [overrideBuiltInParameters boolValue];
        }
        NSNumber* enableAutoIAPEvent = configDict[@"autoIAPEventEnabled"];
        if (enableAutoIAPEvent) {
            eventOptions.enableAutoIAPEvent = [enableAutoIAPEvent boolValue];
        }
        NSNumber* enableAdvertiserIDCollection = configDict[@"enableAdvertiserIDCollection"];
        if (enableAdvertiserIDCollection) {
            eventOptions.enableAdvertiserIDCollection = [enableAdvertiserIDCollection boolValue];
        }

        NSString* propertiesJson = configDict[@"propertiesJson"];
        if (propertiesJson.length > 0) {
            NSDictionary* properties = [self parseJson:propertiesJson];
            if (properties) {
                eventOptions.properties = properties;
            } else {
                NSLog(@"[TapCoreIOSBridge] Warning: propertiesJson is not a valid JSON object");
            }
        }

        NSLog(@"[TapCoreIOSBridge] TapTapEventOptions - channel: %@, overrideBuiltInParameters: %d",
              eventOptions.channel, eventOptions.overrideBuiltInParameters);
    }

    NSMutableArray* otherOptions = [NSMutableArray array];
    [otherOptions addObject:eventOptions];

    // Feature modules own their UE option types and serializers. The Core bridge only
    // resolves the registered native option class and calls its supported JSON factory.
    NSArray* serializedOptions = configDict[@"otherOptions"];
    if ([serializedOptions isKindOfClass:[NSArray class]]) {
        for (NSDictionary* descriptor in serializedOptions) {
            if (![descriptor isKindOfClass:[NSDictionary class]]) continue;

            NSString* nativeClassName = descriptor[@"nativeClass"];
            NSString* optionJson = descriptor[@"json"];
            Class optionClass = NSClassFromString(nativeClassName);
            NSDictionary* values = [self parseJson:optionJson];
            if (!optionClass || !values) {
                NSLog(@"[TapCoreIOSBridge] Module option unavailable: %@", nativeClassName);
                continue;
            }

            // UE iOS builds disable Objective-C exceptions, so @try/@catch cannot compile.
            // Validate the factory before invoking it instead of catching NSException.
            SEL factorySelector = NSSelectorFromString(@"initWithJson:");
            if (![optionClass respondsToSelector:factorySelector]) {
                NSLog(@"[TapCoreIOSBridge] %@ does not implement initWithJson:", nativeClassName);
                continue;
            }

            NSMethodSignature* signature = [optionClass methodSignatureForSelector:factorySelector];
            if (!signature || [signature numberOfArguments] < 3 || strcmp([signature methodReturnType], @encode(id)) != 0) {
                NSLog(@"[TapCoreIOSBridge] Unsupported initWithJson: signature for %@", nativeClassName);
                continue;
            }

            NSInvocation* invocation = [NSInvocation invocationWithMethodSignature:signature];
            [invocation setTarget:optionClass];
            [invocation setSelector:factorySelector];
            NSDictionary* argument = values;
            [invocation setArgument:&argument atIndex:2];
            [invocation invoke];

            __unsafe_unretained id nativeOption = nil;
            [invocation getReturnValue:&nativeOption];
            if (nativeOption) {
                [otherOptions addObject:nativeOption];
            } else {
                NSLog(@"[TapCoreIOSBridge] initWithJson: returned nil for %@", nativeClassName);
            }
        }
    }

    // UE iOS builds disable Objective-C exceptions. Native init failures are delivered
    // through the registered callback below, not by catching a synchronous NSException.
    if (otherOptions.count > 0) {
        [TapTapSDK initWithOptions:options otherOptions:otherOptions];
    } else {
        [TapTapSDK initWithOptions:options otherOptions:nil];
    }

    // Register after init starts its new session; native replays a synchronous final result.
    _initCallback = [TapCoreInitCallback new];
    _initCallback.resultBlock = ^(NSInteger code, NSString* message) {
        _isInitialized = (code == 0 || code == 1002);
        if (result) result(code, message);
    };
    [TapTapSDK addInitCallback:_initCallback];
    NSLog(@"[TapCoreIOSBridge] iOS SDK initialization requested");

    if (completion) {
        dispatch_async(dispatch_get_main_queue(), ^{
            completion(YES, nil);
        });
    }
#else
    // Stub implementation (simulator or SDK not available)
    NSLog(@"[TapCoreIOSBridge] Using stub implementation (SDK not available)");
    if (completion) completion(NO, @"iOS SDK is unavailable");
#endif
}

#pragma mark - SDK Information

+ (NSString*)getSDKVersion
{
#if USE_REAL_SDK
    return [TapTapSDK version];
#else
    return @"Stub-4.0.0";
#endif
}

+ (BOOL)isInitialized
{
    return _isInitialized;
}

#pragma mark - Platform Settings

+ (void)setPlatform:(NSString*)platform version:(NSString*)version
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self setPlatform:platform version:version];
        });
        return;
    }

    NSLog(@"[TapCoreIOSBridge] Setting platform: %@ version: %@", platform, version);

#if USE_REAL_SDK
    // Note: TapTapSDK doesn't have a setPlatform method in current version
    // Platform info is automatically collected by the SDK
    NSLog(@"[TapCoreIOSBridge] Platform info noted (SDK auto-collects this)");
#endif
}

+ (void)updateLanguage:(NSInteger)languageCode
{
    if (![NSThread isMainThread]) {
        dispatch_async(dispatch_get_main_queue(), ^{
            [self updateLanguage:languageCode];
        });
        return;
    }

    NSLog(@"[TapCoreIOSBridge] Updating language to code: %ld", (long)languageCode);

#if USE_REAL_SDK
    TapLanguageType languageType;
    switch (languageCode) {
        case 0:
            languageType = TapLanguageType_Auto;
            break;
        case 1:
            languageType = TapLanguageType_zh_Hans;
            break;
        case 2:
            languageType = TapLanguageType_zh_Hant;
            break;
        case 3:
            languageType = TapLanguageType_en;
            break;
        case 4:
            languageType = TapLanguageType_ja;
            break;
        case 5:
            languageType = TapLanguageType_ko;
            break;
        case 6:
            languageType = TapLanguageType_th;
            break;
        case 7:
            languageType = TapLanguageType_id;
            break;
        default:
            languageType = TapLanguageType_Auto;
            break;
    }

    [TapTapSDK updateLanguage:languageType];
    NSLog(@"[TapCoreIOSBridge] Language updated successfully");
#else
    NSLog(@"[TapCoreIOSBridge] Stub - language update skipped");
#endif
}

#pragma mark - Utility Methods

+ (NSDictionary*)parseJson:(NSString*)jsonString
{
    if (!jsonString || jsonString.length == 0) {
        NSLog(@"[TapCoreIOSBridge] JSON string is empty");
        return nil;
    }

    NSData* jsonData = [jsonString dataUsingEncoding:NSUTF8StringEncoding];
    if (!jsonData) {
        NSLog(@"[TapCoreIOSBridge] Failed to convert JSON string to data");
        return nil;
    }

    NSError* error = nil;
    NSDictionary* jsonDict = [NSJSONSerialization JSONObjectWithData:jsonData
                                                            options:0
                                                              error:&error];

    if (error) {
        NSLog(@"[TapCoreIOSBridge] JSON parsing error: %@", error.localizedDescription);
        return nil;
    }

    if (![jsonDict isKindOfClass:[NSDictionary class]]) {
        NSLog(@"[TapCoreIOSBridge] JSON is not a dictionary");
        return nil;
    }

    return jsonDict;
}

#pragma mark - TapDB / TapEvent

+ (NSDictionary*)_parsePropertiesJson:(NSString*)propertiesJson
{
    if (!propertiesJson || propertiesJson.length == 0) return @{};
    NSDictionary* dict = [self parseJson:propertiesJson];
    return dict ?: @{};
}

+ (void)eventSetUserID:(NSString*)userID
{
#if USE_REAL_SDK
    [TapTapEvent setUserID:userID];
    NSLog(@"[TapCoreIOSBridge] setUserID: %@", userID);
#endif
}

+ (void)eventSetUserID:(NSString*)userID properties:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    NSDictionary* props = [self _parsePropertiesJson:propertiesJson];
    [TapTapEvent setUserID:userID properties:props];
    NSLog(@"[TapCoreIOSBridge] setUserID: %@ with properties", userID);
#endif
}

+ (void)eventClearUser
{
#if USE_REAL_SDK
    [TapTapEvent clearUser];
    NSLog(@"[TapCoreIOSBridge] clearUser");
#endif
}

+ (NSString*)eventGetDeviceId
{
#if USE_REAL_SDK
    NSString* deviceId = [TapTapEvent getDeviceId];
    NSLog(@"[TapCoreIOSBridge] getDeviceId: %@", deviceId);
    return deviceId ?: @"";
#else
    return @"";
#endif
}

+ (void)eventLogEvent:(NSString*)name properties:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    NSDictionary* props = [self _parsePropertiesJson:propertiesJson];
    [TapTapEvent logEvent:name properties:props];
    NSLog(@"[TapCoreIOSBridge] logEvent: %@", name);
#endif
}

+ (void)eventLogPurchasedEvent:(NSString*)orderJson
{
#if USE_REAL_SDK
    NSDictionary* orderDict = [self _parsePropertiesJson:orderJson];
    [TapTapEvent logPurchasedEvent:orderDict[@"orderId"]
                       productName:orderDict[@"productName"]
                            amount:[orderDict[@"amount"] integerValue]
                      currencyType:orderDict[@"currencyType"]
                     paymentMethod:orderDict[@"paymentMethod"]
                        properties:orderDict[@"properties"]];
    NSLog(@"[TapCoreIOSBridge] logPurchasedEvent: %@", orderDict[@"orderId"]);
#endif
}

+ (void)eventLogDeviceLoginEvent
{
#if USE_REAL_SDK
    [TapTapEvent logEvent:@"device_login" properties:@{}];
    NSLog(@"[TapCoreIOSBridge] logDeviceLoginEvent");
#endif
}

+ (void)eventDeviceInitialize:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    [TapTapEvent deviceInitialize:[self _parsePropertiesJson:propertiesJson]];
    NSLog(@"[TapCoreIOSBridge] deviceInitialize");
#endif
}

+ (void)eventDeviceUpdate:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    [TapTapEvent deviceUpdate:[self _parsePropertiesJson:propertiesJson]];
    NSLog(@"[TapCoreIOSBridge] deviceUpdate");
#endif
}

+ (void)eventDeviceAdd:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    [TapTapEvent deviceAdd:[self _parsePropertiesJson:propertiesJson]];
    NSLog(@"[TapCoreIOSBridge] deviceAdd");
#endif
}

+ (void)eventUserInitialize:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    [TapTapEvent userInitialize:[self _parsePropertiesJson:propertiesJson]];
    NSLog(@"[TapCoreIOSBridge] userInitialize");
#endif
}

+ (void)eventUserUpdate:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    [TapTapEvent userUpdate:[self _parsePropertiesJson:propertiesJson]];
    NSLog(@"[TapCoreIOSBridge] userUpdate");
#endif
}

+ (void)eventUserAdd:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    [TapTapEvent userAdd:[self _parsePropertiesJson:propertiesJson]];
    NSLog(@"[TapCoreIOSBridge] userAdd");
#endif
}

+ (void)eventAddCommonProperty:(NSString*)key value:(NSString*)value
{
#if USE_REAL_SDK
    [TapTapEvent addCommonProperty:key value:value];
    NSLog(@"[TapCoreIOSBridge] addCommonProperty: %@ = %@", key, value);
#endif
}

+ (void)eventAddCommon:(NSString*)propertiesJson
{
#if USE_REAL_SDK
    [TapTapEvent addCommon:[self _parsePropertiesJson:propertiesJson]];
    NSLog(@"[TapCoreIOSBridge] addCommon");
#endif
}

+ (void)eventClearCommonProperty:(NSString*)key
{
#if USE_REAL_SDK
    [TapTapEvent clearCommonProperty:key];
    NSLog(@"[TapCoreIOSBridge] clearCommonProperty: %@", key);
#endif
}

+ (void)eventClearCommonProperties:(NSArray<NSString*>*)keys
{
#if USE_REAL_SDK
    [TapTapEvent clearCommonProperties:keys];
    NSLog(@"[TapCoreIOSBridge] clearCommonProperties: %lu keys", (unsigned long)keys.count);
#endif
}

+ (void)eventClearAllCommonProperties
{
#if USE_REAL_SDK
    [TapTapEvent clearAllCommonProperties];
    NSLog(@"[TapCoreIOSBridge] clearAllCommonProperties");
#endif
}

+ (void)eventRegisterDynamicProperties:(NSDictionary* (^)(void))block
{
#if USE_REAL_SDK
    [TapTapEvent registerDynamicProperties:block];
    NSLog(@"[TapCoreIOSBridge] Dynamic properties registered");
#else
    NSLog(@"[TapCoreIOSBridge] Stub - dynamic properties registration skipped");
#endif
}

@end

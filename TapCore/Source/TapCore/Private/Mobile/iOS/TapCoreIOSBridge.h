// Copyright TapTap. All Rights Reserved.

#import <Foundation/Foundation.h>

/**
 * Pure Objective-C bridge for TapCore iOS SDK
 * This class provides a clean Objective-C interface between UE5 Objective-C++ code
 * and the native iOS TapTapSDK.
 *
 * Architecture:
 *   C++ (TapCore.cpp) -> Objective-C++ (TapCoreiOS.mm) -> Pure ObjC (TapCoreIOSBridge) -> iOS SDK
 */
@interface TapCoreIOSBridge : NSObject

/**
 * Initialize TapTapSDK with configuration JSON
 *
 * @param configJson JSON string containing configuration (clientId, clientToken, region, etc.)
 * @param completion Callback block reporting whether the native request was accepted
 *                   - success: YES if the native request was accepted, NO otherwise
 *                   - errorMsg: Error message if initialization failed, nil otherwise
 * @param result Final native initialization result, delivered by a one-shot listener
 */
+ (void)initWithConfig:(NSString* _Nonnull)configJson
            completion:(void(^ _Nullable)(BOOL success, NSString* _Nullable errorMsg))completion
           finalResult:(void(^ _Nonnull)(NSInteger code, NSString* _Nullable message))result;

/**
 * Get the iOS SDK version string
 *
 * @return Version string (e.g., "3.29.0")
 */
+ (NSString* _Nonnull)getSDKVersion;

/**
 * Set platform information for analytics
 *
 * @param platform Platform name (e.g., "UnrealEngine")
 * @param version Platform version (e.g., "5.3.2")
 */
+ (void)setPlatform:(NSString* _Nonnull)platform version:(NSString* _Nonnull)version;

/**
 * Update SDK language setting
 *
 * @param languageCode Language code (0=Auto, 1=SimplifiedChinese, 2=TraditionalChinese, 3=English, etc.)
 */
+ (void)updateLanguage:(NSInteger)languageCode;

/**
 * Check if SDK is initialized
 *
 * @return YES if initialized, NO otherwise
 */
+ (BOOL)isInitialized;

/**
 * Parse JSON string to NSDictionary (utility method)
 */
+ (NSDictionary* _Nullable)parseJson:(NSString* _Nullable)jsonString;

#pragma mark - TapDB / TapEvent

+ (void)eventSetUserID:(NSString* _Nonnull)userID;
+ (void)eventSetUserID:(NSString* _Nonnull)userID properties:(NSString* _Nullable)propertiesJson;
+ (void)eventClearUser;
+ (NSString* _Nonnull)eventGetDeviceId;

+ (void)eventLogEvent:(NSString* _Nonnull)name properties:(NSString* _Nullable)propertiesJson;
+ (void)eventLogPurchasedEvent:(NSString* _Nonnull)orderJson;
+ (void)eventLogDeviceLoginEvent;

+ (void)eventDeviceInitialize:(NSString* _Nullable)propertiesJson;
+ (void)eventDeviceUpdate:(NSString* _Nullable)propertiesJson;
+ (void)eventDeviceAdd:(NSString* _Nullable)propertiesJson;

+ (void)eventUserInitialize:(NSString* _Nullable)propertiesJson;
+ (void)eventUserUpdate:(NSString* _Nullable)propertiesJson;
+ (void)eventUserAdd:(NSString* _Nullable)propertiesJson;

+ (void)eventAddCommonProperty:(NSString* _Nonnull)key value:(NSString* _Nonnull)value;
+ (void)eventAddCommon:(NSString* _Nullable)propertiesJson;
+ (void)eventClearCommonProperty:(NSString* _Nonnull)key;
+ (void)eventClearCommonProperties:(NSArray<NSString*>* _Nonnull)keys;
+ (void)eventClearAllCommonProperties;

+ (void)eventRegisterDynamicProperties:(NSDictionary* _Nonnull (^ _Nonnull)(void))block;

@end

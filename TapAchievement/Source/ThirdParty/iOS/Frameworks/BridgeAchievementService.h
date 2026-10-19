//
//  BridgeAchievementService.h
//  TapAchievement
//
//  ObjC interface declaration for BridgeAchievementService.
//  Implementation is provided by TapTapAchievementSDK (xcframework or CocoaPods).
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface BridgeAchievementService: NSObject

/// unlock achievement
+ (void) achievementId:(NSString *) achievementId;

/// increment achievement steps
+ (void) achievementId:(NSString *) achievementId steps:(NSNumber *) steps;

+ (void) showAchievements;

+ (void) enableToast:(NSNumber *) enableToast;

+ (void) registerCallback:(void (^)(NSString *result))callback;

@end

NS_ASSUME_NONNULL_END

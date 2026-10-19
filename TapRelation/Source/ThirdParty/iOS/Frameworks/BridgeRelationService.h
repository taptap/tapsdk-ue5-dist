//
//  BridgeRelationService.h
//  TapTapRelationSDK
//
//  Created by SeraphLi on 2025/5/21.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface BridgeRelationService : NSObject

+ (void) startMessenger;

+ (void) prepare;

+ (void) inviteGame;

+ (void) teamId :(NSString *) teamId;

+ (void) openId:(NSString *) openId unionId:(NSString *) unionId;

+ (void) getNewFansCount:(void (^)(NSString *result))callback;

+ (void) getUnreadMessageCount:(void (^)(NSString *result))callback;

+ (void) registerRelationCallback:(void (^)(NSString *result))callback;

+ (void) unregisterRelationCallback;

+ (void) registerRelationInviteCallback:(void (^)(NSString *result))callback;

+ (void) unregisterRelationInviteCallback;

+ (void) destroy;

@end

NS_ASSUME_NONNULL_END

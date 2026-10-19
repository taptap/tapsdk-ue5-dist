//
//  BridgeRelationLiteService.h
//  TapTapRelationLiteSDK
//
//  Created by SeraphLi on 2025/8/26.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface BridgeRelationLiteService : NSObject

+ (void) registerRelationLiteCallback:(void (^)(NSString *result))callback;

+ (void) unregisterRelationLiteCallback;

+ (void) unregisterRelationLiteInviteCallback;

+ (void) inviteGameByRelationLite;

+ (void) relationLiteTeamId:(NSString *) teamId;

+ (void) relationLiteOpenId:(NSString *) openId unionId:(NSString *) unionId;

+ (void) registerRelationLiteInviteCallback:(void (^)(NSString *result))callback;

+ (void) friendListNextPageToken:(NSString *) nextPageToken bridgeCallback: (void (^)(NSString *result))callback;

+ (void) followingListNextPageToken:(NSString *) nextPageToken bridgeCallback: (void (^)(NSString *result))callback;

+ (void) fansListNextPageToken:(NSString *) nextPageToken bridgeCallback: (void (^)(NSString *result))callback;

+ (void) syncRelationAction:(NSNumber *) action nickname:(NSString *) nickname friendNickname
                           :(NSString *) friendNickname friendOpenId: (NSString *) friendOpenId  bridgeCallback: (void (^)(NSString *result))callback;

+ (void) syncRelationAction:(NSNumber *) action nickname:(NSString *) nickname friendNickname
                           :(NSString *) friendNickname friendUnionId:(NSString *) friendUnionId  bridgeCallback: (void (^)(NSString *result))callback;
@end

NS_ASSUME_NONNULL_END

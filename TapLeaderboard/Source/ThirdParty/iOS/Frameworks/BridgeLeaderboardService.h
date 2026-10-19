//
//  BridgeLeaderboardService.h
//  TapTapLeaderboard
//
//  Created on 2024/12/31.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^BridgeLeaderboardResultCallback)(NSString *result);

@interface BridgeLeaderboardService : NSObject

/// 打开用户名片
/// - Parameters:
///   - openId: 开放平台ID
///   - unionId: 统一ID
+ (void)openId:(NSString *)openId unionId:(NSString *)unionId;

/// 打开排行榜
/// - Parameters:
///   - leaderboardId: 排行榜ID
///   - collection: 排行榜集合类型
+ (void)leaderboardId:(NSString *)leaderboardId leaderboardCollection:(NSString *)leaderboardCollection;

/// 批量提交用户排行榜分数
/// - Parameters:
///   - scores: 分数列表JSON字符串
///   - callback: 结果回调
+ (void)scores:(NSString *)scores callback:(BridgeLeaderboardResultCallback)callback;

/// 获取排行榜数据
/// - Parameters:
///   - leaderboardId: 排行榜ID
///   - collection: 排行榜集合类型
///   - nextPage: 下一页标识
///   - periodToken: 周期令牌
///   - callback: 结果回调
+ (void)leaderboardId:(NSString *)leaderboardId 
        leaderboardCollection:(NSString *)leaderboardCollection
                     nextPage:(NSString * _Nullable)nextPage
                  periodToken:(NSString * _Nullable)periodToken 
                     callback:(BridgeLeaderboardResultCallback)callback;

/// 单查用户排行榜数据
/// - Parameters:
///   - leaderboardId: 排行榜ID
///   - collection: 排行榜集合类型
///   - periodToken: 周期令牌
///   - callback: 结果回调
+ (void)leaderboardId:(NSString *)leaderboardId
                    leaderboardCollection:(NSString *)leaderboardCollection
                              periodToken:(NSString * _Nullable)periodToken
                                 callback:(BridgeLeaderboardResultCallback)callback;

/// 查询用户相近用户成绩
/// - Parameters:
///   - leaderboardId: 排行榜ID
///   - collection: 排行榜集合类型
///   - periodToken: 周期令牌
///   - maxCount: 最大数量
///   - callback: 结果回调
+ (void)leaderboardId:(NSString *)leaderboardId 
            leaderboardCollection:(NSString *)leaderboardCollection
                     periodToken:(NSString * _Nullable)periodToken
                        maxCount:(NSNumber * _Nullable)maxCount 
                        callback:(BridgeLeaderboardResultCallback)callback;

/// 注册排行榜状态回调
/// - Parameter callback: 状态回调
+ (void)registerLeaderboardCallback:(BridgeLeaderboardResultCallback)callback;

/// 取消注册排行榜回调
+ (void)unregisterLeaderboardCallback;

/// 设置分享回调
/// - Parameter callback: 分享结果回调
+ (void)setShareCallback:(BridgeLeaderboardResultCallback _Nullable)callback;

/// 获取分享回调状态
+ (BOOL)hasShareCallback;

@end

NS_ASSUME_NONNULL_END

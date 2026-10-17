// Copyright TapTap. All Rights Reserved.
#include "iOS/TapRelationIOSBridge.h"
#include "TapRelationInviteParser.h"
#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#include "TapRelationModule.h"
#include "Async/Async.h"
#include "IOS/IOSAppDelegate.h"
#include "HAL/CriticalSection.h"

#if TAP_RELATION_IOS_SDK_AVAILABLE
#import <TapTapRelationSDK/TapTapRelationSDK-Swift.h>
#import "BridgeRelationService.h"
#endif

namespace
{
    NSString* ToNS(const FString& S) { return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*S)]; }

    FCriticalSection GLock;
    TapRelationIOSBridge::FRelationCallback GPersistentCallback;
    TapRelationIOSBridge::FInviteCallback GInviteCallback;
    bool GInitialized = false;

    void ParseAndDispatch(NSString* ResultStr, TapRelationIOSBridge::FRelationCallback Cb)
    {
        if (!ResultStr || !Cb) return;
        NSData* Data = [ResultStr dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary* D = [NSJSONSerialization JSONObjectWithData:Data options:0 error:nil];
        int32 Code = 0, Fans = 0, Unread = 0;
        if (D)
        {
            NSString* CodeStr = D[@"relation_code"];
            if (CodeStr) Code = [CodeStr intValue];
            NSNumber* FansNum = D[@"new_fans_count"];
            if (FansNum) Fans = [FansNum intValue];
            NSNumber* UnreadNum = D[@"unread_message_count"];
            if (UnreadNum) Unread = [UnreadNum intValue];
        }
        AsyncTask(ENamedThreads::GameThread, [Cb, Code, Fans, Unread]() { Cb(Code, Fans, Unread); });
    }

    int32 ParseIntField(NSString* ResultStr, NSString* Key)
    {
        if (!ResultStr) return 0;
        NSData* Data = [ResultStr dataUsingEncoding:NSUTF8StringEncoding];
        NSDictionary* D = [NSJSONSerialization JSONObjectWithData:Data options:0 error:nil];
        NSNumber* Num = D[Key];
        return Num ? [Num intValue] : 0;
    }
}

namespace TapRelationIOSBridge
{
    namespace
    {
        FDelegateHandle OpenURLHandle;

        void HandleOpenURL(UIApplication*, NSURL* URL, NSString*, id)
        {
#if TAP_RELATION_IOS_SDK_AVAILABLE
            if (URL)
            {
                dispatch_async(dispatch_get_main_queue(), ^{
                    [TapTapRelation openWithUrl:URL];
                });
            }
#endif
        }
    }

    void RegisterOpenURLHandler()
    {
        if (!OpenURLHandle.IsValid())
        {
            OpenURLHandle = FIOSCoreDelegates::OnOpenURL.AddStatic(&HandleOpenURL);
        }
    }

    void UnregisterOpenURLHandler()
    {
        FIOSCoreDelegates::OnOpenURL.Remove(OpenURLHandle);
        OpenURLHandle.Reset();
    }

    void Init(FRelationCallback OnPersistentResult)
    {
        { FScopeLock Lock(&GLock); GPersistentCallback = MoveTemp(OnPersistentResult); GInitialized = true; }
        UE_LOG(LogTapRelation, Log, TEXT("[TapRelation-iOS] Bridge initialized"));
    }

    void Prepare()
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService prepare];
#endif
    }

    // StartMessenger/InviteGame/InviteTeam/ShowTapUserProfile 最终会创建并 present
    // UIViewController（UINavigationController(rootViewController:) 等），必须切回主线程执行。
    // UE5 iOS 的游戏线程（IOSAppDelegate MainAppThread）不是 UIKit 认定的主线程，
    // 直接在其上做 UIKit/AutoLayout 操作会在 UINavigationController 初始化时
    // 于 _setUpContentFocusContainerGuide 内抛出无法定位原因的 NSLayoutConstraint 异常崩溃。
    void StartMessenger()
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        dispatch_async(dispatch_get_main_queue(), ^{
            [BridgeRelationService startMessenger];
        });
#endif
    }

    void InviteGame()
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        dispatch_async(dispatch_get_main_queue(), ^{
            [BridgeRelationService inviteGame];
        });
#endif
    }

    void InviteTeam(const FString& TeamId)
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        NSString* TeamIdNS = ToNS(TeamId);
        dispatch_async(dispatch_get_main_queue(), ^{
            [BridgeRelationService teamId:TeamIdNS];
        });
#endif
    }

    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId)
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        NSString* OpenIdNS = ToNS(OpenId);
        NSString* UnionIdNS = ToNS(UnionId);
        dispatch_async(dispatch_get_main_queue(), ^{
            [BridgeRelationService openId:OpenIdNS unionId:UnionIdNS];
        });
#endif
    }

    void GetNewFansCount(TFunction<void(int32)> Callback)
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService getNewFansCount:^(NSString* result)
        {
            int32 Count = ParseIntField(result, @"new_fans_count");
            AsyncTask(ENamedThreads::GameThread, [Callback, Count]() { if (Callback) Callback(Count); });
        }];
#else
        if (Callback) Callback(0);
#endif
    }

    void GetUnreadMessageCount(TFunction<void(int32)> Callback)
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService getUnreadMessageCount:^(NSString* result)
        {
            int32 Count = ParseIntField(result, @"unread_message_count");
            AsyncTask(ENamedThreads::GameThread, [Callback, Count]() { if (Callback) Callback(Count); });
        }];
#else
        if (Callback) Callback(0);
#endif
    }

    void RegisterRelationCallback()
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService registerRelationCallback:^(NSString* result)
        {
            TapRelationIOSBridge::FRelationCallback Snapshot;
            { FScopeLock Lock(&GLock); Snapshot = GPersistentCallback; }
            if (Snapshot) ParseAndDispatch(result, Snapshot);
        }];
        UE_LOG(LogTapRelation, Log, TEXT("[TapRelation-iOS] registerRelationCallback registered"));
#else
        UE_LOG(LogTapRelation, Warning, TEXT("[TapRelation-iOS] SDK not available"));
#endif
    }
    void UnregisterRelationCallback()
    {
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService unregisterRelationCallback];
#endif
    }
    void RegisterRelationInviteCallback(FInviteCallback Callback)
    {
        { FScopeLock Lock(&GLock); GInviteCallback=MoveTemp(Callback); }
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService registerRelationInviteCallback:^(NSString* result){
            if (!result) return;
            FString O, U, T;
            if (!TapRelationInviteParser::Parse(UTF8_TO_TCHAR([result UTF8String]), O, U, T)) return;
            FInviteCallback C; { FScopeLock Lock(&GLock); C=GInviteCallback; } if(C) AsyncTask(ENamedThreads::GameThread,[C,O,U,T](){C(O,U,T);});
        }];
#endif
    }
    void UnregisterRelationInviteCallback()
    {
        { FScopeLock Lock(&GLock); GInviteCallback=nullptr; }
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService unregisterRelationInviteCallback];
#endif
    }

    void Destroy()
    {
        UnregisterRelationCallback();
        UnregisterRelationInviteCallback();
#if TAP_RELATION_IOS_SDK_AVAILABLE
        [BridgeRelationService destroy];
#endif
    }
}
#endif

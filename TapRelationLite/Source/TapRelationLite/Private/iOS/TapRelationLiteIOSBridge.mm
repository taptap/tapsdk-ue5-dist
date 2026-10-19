// Copyright TapTap. All Rights Reserved.
#include "iOS/TapRelationLiteIOSBridge.h"
#include "TapRelationLiteInviteParser.h"
#include "TapRelationLiteListResult.h"
#include "TapRelationLiteCallbackUtils.h"
#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#include "TapRelationLiteModule.h"
#include "Async/Async.h"
#include "IOS/IOSAppDelegate.h"
#include "HAL/CriticalSection.h"

#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
#import <TapTapRelationLiteSDK/TapTapRelationLiteSDK-Swift.h>
#endif

namespace
{
    NSString* ToNS(const FString& S) { return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*S)]; }
    FString FromNS(NSString* S) { return S ? FString(UTF8_TO_TCHAR([S UTF8String])) : FString(); }

    FCriticalSection GLock;
    TFunction<void(int32)> GPersistentCallback;
    TapRelationLiteIOSBridge::FInviteCallback GInviteCallback;
    FTapRelationLiteCallbackGeneration GLifetime;
    FTapRelationLiteCallbackGeneration GRelationGeneration;
    FTapRelationLiteCallbackGeneration GInviteGeneration;
}

namespace TapRelationLiteIOSBridge
{
    namespace
    {
        FDelegateHandle OpenURLHandle;

        void HandleOpenURL(UIApplication*, NSURL* URL, NSString*, id)
        {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
            if (URL)
            {
                const auto IsCurrent = GLifetime.Capture();
                dispatch_async(dispatch_get_main_queue(), ^{
                    if (IsCurrent()) [TapTapRelationLite openWithUrl:URL];
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

    void Init(TFunction<void(int32)> OnPersistentResult)
    {
        GLifetime.Invalidate();
        FScopeLock Lock(&GLock);
        GPersistentCallback = MoveTemp(OnPersistentResult);
        UE_LOG(LogTapRelationLite, Log, TEXT("[TapRelationLite-iOS] Bridge initialized"));
    }

    void Shutdown()
    {
        GLifetime.Invalidate();
        UnregisterRelationLiteCallback();
        UnregisterRelationLiteInviteCallback();
        FScopeLock Lock(&GLock);
        GPersistentCallback = nullptr;
    }

    // InviteGame/InviteTeam/ShowTapUserProfile 最终会创建并 present UIViewController，
    // 必须切回主线程执行。UE5 iOS 的游戏线程（IOSAppDelegate MainAppThread）不是
    // UIKit 认定的主线程，直接在其上做 UIKit/AutoLayout 操作会触发无法定位原因的
    // 内部 NSLayoutConstraint 异常崩溃（与 TapRelation 同构问题）。
    void InviteGame()
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (IsCurrent()) [[TapRelationLiteSwiftBridgeService shared] inviteGameByRelationLite];
        });
#endif
    }

    void InviteTeam(const FString& TeamId)
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        NSString* TeamIdNS = ToNS(TeamId);
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (IsCurrent()) [[TapRelationLiteSwiftBridgeService shared] inviteTeamWithTeamId:TeamIdNS];
        });
#endif
    }

    void ShowTapUserProfile(const FString& OpenId, const FString& UnionId)
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        NSString* OpenIdNS = ToNS(OpenId);
        NSString* UnionIdNS = ToNS(UnionId);
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (IsCurrent()) [[TapRelationLiteSwiftBridgeService shared] showTapUserProfileWithOpenId:OpenIdNS unionId:UnionIdNS];
        });
#endif
    }

    void GetFriendsList(const FString& Token, FListSuccess OnSuccess, FListFailure OnFailure)
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        NSString* TokenNS = ToNS(Token);
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!IsCurrent()) return;
            [[TapRelationLiteSwiftBridgeService shared] getFriendsListWithNextPageToken:TokenNS completion:^(NSString* result)
            {
                TapRelationLiteListResult::Complete(FromNS(result), TEXT("friends_list"), OnSuccess, OnFailure, IsCurrent);
            }];
        });
#else
        if (OnFailure) AsyncTask(ENamedThreads::GameThread, [OnFailure]() { OnFailure(-1, TEXT("SDK not available")); });
#endif
    }

    void GetFollowingList(const FString& Token, FListSuccess OnSuccess, FListFailure OnFailure)
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        NSString* TokenNS = ToNS(Token);
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!IsCurrent()) return;
            [[TapRelationLiteSwiftBridgeService shared] getFollowingListWithNextPageToken:TokenNS completion:^(NSString* result)
            {
                TapRelationLiteListResult::Complete(FromNS(result), TEXT("following_list"), OnSuccess, OnFailure, IsCurrent);
            }];
        });
#else
        if (OnFailure) AsyncTask(ENamedThreads::GameThread, [OnFailure]() { OnFailure(-1, TEXT("SDK not available")); });
#endif
    }

    void GetFansList(const FString& Token, FListSuccess OnSuccess, FListFailure OnFailure)
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        NSString* TokenNS = ToNS(Token);
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!IsCurrent()) return;
            [[TapRelationLiteSwiftBridgeService shared] getFansListWithNextPageToken:TokenNS completion:^(NSString* result)
            {
                TapRelationLiteListResult::Complete(FromNS(result), TEXT("fans_list"), OnSuccess, OnFailure, IsCurrent);
            }];
        });
#else
        if (OnFailure) AsyncTask(ENamedThreads::GameThread, [OnFailure]() { OnFailure(-1, TEXT("SDK not available")); });
#endif
    }

    void SyncRelationshipWithOpenId(int32 Action, const FString& N, const FString& FN, const FString& OpenId, FSyncSuccess OnSuccess, FSyncFailure OnFailure)
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        NSString* Nickname = ToNS(N);
        NSString* FriendNickname = ToNS(FN);
        NSString* Id = ToNS(OpenId);
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!IsCurrent()) return;
            [[TapRelationLiteSwiftBridgeService shared] syncRelationshipWithOpenIdWithAction:Action nickname:Nickname friendNickname:FriendNickname friendOpenId:Id completion:^(NSString* result)
            {
                TapRelationLiteCallbackUtils::CompleteSync(FromNS(result), OnSuccess, OnFailure, IsCurrent);
            }];
        });
#else
        if (OnFailure) AsyncTask(ENamedThreads::GameThread, [OnFailure]() { OnFailure(TEXT("SDK not available")); });
#endif
    }

    void SyncRelationshipWithUnionId(int32 Action, const FString& N, const FString& FN, const FString& UnionId, FSyncSuccess OnSuccess, FSyncFailure OnFailure)
    {
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        NSString* Nickname = ToNS(N);
        NSString* FriendNickname = ToNS(FN);
        NSString* Id = ToNS(UnionId);
        const auto IsCurrent = GLifetime.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!IsCurrent()) return;
            [[TapRelationLiteSwiftBridgeService shared] syncRelationshipWithUnionIdWithAction:Action nickname:Nickname friendNickname:FriendNickname friendUnionId:Id completion:^(NSString* result)
            {
                TapRelationLiteCallbackUtils::CompleteSync(FromNS(result), OnSuccess, OnFailure, IsCurrent);
            }];
        });
#else
        if (OnFailure) AsyncTask(ENamedThreads::GameThread, [OnFailure]() { OnFailure(TEXT("SDK not available")); });
#endif
    }

    void RegisterRelationLiteCallback()
    {
        GRelationGeneration.Invalidate();
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        const auto IsCurrent = GRelationGeneration.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!IsCurrent()) return;
            // Also remove the previous proxy when using SDK artifacts predating native deduplication.
            [[TapRelationLiteSwiftBridgeService shared] unregisterRelationLiteCallback];
            [[TapRelationLiteSwiftBridgeService shared] registerRelationLiteCallbackWithCompletion:^(NSString* result)
            {
                int32 Code = 0;
                if (!IsCurrent() || !TapRelationLiteCallbackUtils::ParseResultCode(FromNS(result), Code)) return;
                TapRelationLiteCallbackUtils::Dispatch([Code]
                {
                    TFunction<void(int32)> Callback;
                    { FScopeLock Lock(&GLock); Callback = GPersistentCallback; }
                    if (Callback) Callback(Code);
                }, IsCurrent);
            }];
        });
        UE_LOG(LogTapRelationLite, Log, TEXT("[TapRelationLite-iOS] registerRelationLiteCallback registered"));
#else
        UE_LOG(LogTapRelationLite, Warning, TEXT("[TapRelationLite-iOS] SDK not available"));
#endif
    }

    void UnregisterRelationLiteCallback()
    {
        GRelationGeneration.Invalidate();
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        const auto IsCurrent = GRelationGeneration.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (IsCurrent()) [[TapRelationLiteSwiftBridgeService shared] unregisterRelationLiteCallback];
        });
#endif
    }
    void RegisterRelationLiteInviteCallback(FInviteCallback Callback)
    {
        GInviteGeneration.Invalidate();
        {FScopeLock Lock(&GLock);GInviteCallback=MoveTemp(Callback);}
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        const auto IsCurrent = GInviteGeneration.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (!IsCurrent()) return;
            [[TapRelationLiteSwiftBridgeService shared] unregisterRelationLiteInviteCallback];
            [[TapRelationLiteSwiftBridgeService shared] registerRelationLiteInviteCallbackWithCompletion:^(NSString* result)
            {
                FString OpenId, UnionId, TeamId;
                if (!IsCurrent() || !TapRelationLiteInviteParser::Parse(FromNS(result), OpenId, UnionId, TeamId)) return;
                TapRelationLiteCallbackUtils::Dispatch([OpenId, UnionId, TeamId]
                {
                    FInviteCallback Callback;
                    { FScopeLock Lock(&GLock); Callback = GInviteCallback; }
                    if (Callback) Callback(OpenId, UnionId, TeamId);
                }, IsCurrent);
            }];
        });
#endif
    }
    void UnregisterRelationLiteInviteCallback()
    {
        GInviteGeneration.Invalidate();
        {FScopeLock Lock(&GLock);GInviteCallback=nullptr;}
#if TAP_RELATIONLITE_IOS_SDK_AVAILABLE
        const auto IsCurrent = GInviteGeneration.Capture();
        dispatch_async(dispatch_get_main_queue(), ^{
            if (IsCurrent()) [[TapRelationLiteSwiftBridgeService shared] unregisterRelationLiteInviteCallback];
        });
#endif
    }
}
#endif

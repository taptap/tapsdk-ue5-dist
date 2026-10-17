// Copyright TapTap. All Rights Reserved.
#include "iOS/TapShareIOSBridge.h"

#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#include "TapShareModule.h"
#include "Async/Async.h"

#if TAP_SHARE_IOS_SDK_AVAILABLE
#import <TapTapShareSDK/TapTapShareSDK-Swift.h>
#endif

namespace
{
    NSString* ToNS(const FString& S) { return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*S)]; }
}

namespace TapShareIOSBridge
{
    void Share(const FTapShareContent& Content, FShareCallback OnResult)
    {
#if TAP_SHARE_IOS_SDK_AVAILABLE
        TapTapShareObj* Obj = [[TapTapShareObj alloc] init];
        if (!Content.Title.IsEmpty())        { Obj.title = ToNS(Content.Title); }
        if (!Content.Contents.IsEmpty())     { Obj.contents = ToNS(Content.Contents); }
        if (!Content.GroupLabelId.IsEmpty()) { Obj.groupLabelId = ToNS(Content.GroupLabelId); }
        if (!Content.HashtagIds.IsEmpty())   { Obj.hashtagIds = ToNS(Content.HashtagIds); }
        if (!Content.FailUrl.IsEmpty())      { Obj.failUrl = ToNS(Content.FailUrl); }
        if (!Content.BackUrl.IsEmpty())      { Obj.backUrl = ToNS(Content.BackUrl); }

        if (Content.ImagePaths.Num() > 0)
        {
            NSMutableArray<NSData*>* Images = [NSMutableArray array];
            for (const FString& Path : Content.ImagePaths)
            {
                NSData* Data = [NSData dataWithContentsOfFile:ToNS(Path)];
                if (Data) { [Images addObject:Data]; }
            }
            if (Images.count > 0) { Obj.footerImages = Images; }
        }

        FShareCallback Snapshot = MoveTemp(OnResult);
        // share:completion: 最终会 present 分享 UI，必须切回主线程执行。UE5 iOS 的游戏线程
        // （IOSAppDelegate MainAppThread）不是 UIKit 认定的主线程，直接在其上做 UIKit
        // 操作会触发无法定位原因的内部异常崩溃（与 TapRelation/TapMoment 同构问题）。
        dispatch_async(dispatch_get_main_queue(), ^{
            [TapTapShare share:Obj completion:^(NSInteger result)
            {
                const int32 Code = static_cast<int32>(result);
                AsyncTask(ENamedThreads::GameThread, [Snapshot, Code]() { if (Snapshot) Snapshot(Code); });
            }];
        });
#else
        UE_LOG(LogTapShare, Warning, TEXT("[TapShare-iOS] SDK not available: TapTapShareSDK.xcframework missing"));
        const int32 Code = ETapShareResult::ERROR_NOT_SUPPORTED;
        FShareCallback Snapshot = MoveTemp(OnResult);
        AsyncTask(ENamedThreads::GameThread, [Snapshot, Code]() { if (Snapshot) Snapshot(Code); });
#endif
    }
}

#endif // PLATFORM_IOS

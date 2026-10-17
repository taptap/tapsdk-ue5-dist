// Copyright TapTap. All Rights Reserved.
#include "iOS/TapMomentIOSBridge.h"

#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#include "TapMomentModule.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"

#if TAP_MOMENT_IOS_SDK_AVAILABLE
#import <TapTapMomentSDK/TapTapMoment.h>
#import <TapTapMomentSDK/TapTapMomentDelegate.h>
#import <TapTapMomentSDK/TapTapMomentPostData.h>
#endif

// ── 文件级静态状态（ObjC delegate 需要在全局作用域访问）────────────────────
static FCriticalSection              GMomentLock;
static TapMomentIOSBridge::FMomentCallback GMomentCallback;
static bool                          GMomentInitialized = false;

#if TAP_MOMENT_IOS_SDK_AVAILABLE
// ObjC 声明必须位于全局作用域，不能嵌套在 C++ namespace 内
@interface TapMomentDelegateImpl : NSObject <TapTapMomentDelegate>
@end

@implementation TapMomentDelegateImpl
- (void)onMomentCallbackWithCode:(NSInteger)code msg:(NSString *)msg
{
    const int32 Code = static_cast<int32>(code);
    const FString Msg = msg ? FString(UTF8_TO_TCHAR([msg UTF8String])) : FString();
    TapMomentIOSBridge::FMomentCallback Snapshot;
    {
        FScopeLock Inner(&GMomentLock);
        Snapshot = GMomentCallback;
    }
    if (Snapshot)
    {
        AsyncTask(ENamedThreads::GameThread, [Snapshot, Code, Msg]() { Snapshot(Code, Msg); });
    }
}
@end

static TapMomentDelegateImpl* GDelegate = nil;
#endif

namespace
{
    NSString* ToNS(const FString& S) { return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*S)]; }
    FString FromNS(NSString* S) { return S ? FString(UTF8_TO_TCHAR([S UTF8String])) : FString(); }
}

namespace TapMomentIOSBridge
{
    void Init(FMomentCallback OnCallback)
    {
        {
            FScopeLock Lock(&GMomentLock);
            if (GMomentInitialized) { return; }
            GMomentInitialized = true;
            GMomentCallback = MoveTemp(OnCallback);
        }

#if TAP_MOMENT_IOS_SDK_AVAILABLE
        GDelegate = [[TapMomentDelegateImpl alloc] init];
        [TapTapMoment setDelegate:GDelegate];
        UE_LOG(LogTapMoment, Log, TEXT("[TapMoment-iOS] Delegate registered"));
#else
        UE_LOG(LogTapMoment, Warning, TEXT("[TapMoment-iOS] SDK not available: TapTapMomentSDK.xcframework missing"));
#endif
    }

    // Open/OpenScene/Close/CloseWithTitle/FetchNotification/Publish 最终会创建并 present
    // UIViewController，必须切回主线程执行。UE5 iOS 的游戏线程（IOSAppDelegate
    // MainAppThread）不是 UIKit 认定的主线程，直接在其上做 UIKit/AutoLayout 操作会
    // 触发无法定位原因的内部 NSLayoutConstraint 异常崩溃（与 TapRelation 同构问题）。
    void Open()
    {
#if TAP_MOMENT_IOS_SDK_AVAILABLE
        dispatch_async(dispatch_get_main_queue(), ^{
            [TapTapMoment open];
        });
#endif
    }

    void OpenScene(const FString& SceneId)
    {
#if TAP_MOMENT_IOS_SDK_AVAILABLE
        NSString* SceneIdNS = ToNS(SceneId);
        dispatch_async(dispatch_get_main_queue(), ^{
            [TapTapMoment openScene:SceneIdNS];
        });
#endif
    }

    void Close()
    {
#if TAP_MOMENT_IOS_SDK_AVAILABLE
        dispatch_async(dispatch_get_main_queue(), ^{
            [TapTapMoment close];
        });
#endif
    }

    void CloseWithTitle(const FString& Title, const FString& Content)
    {
#if TAP_MOMENT_IOS_SDK_AVAILABLE
        NSString* TitleNS = ToNS(Title);
        NSString* ContentNS = ToNS(Content);
        dispatch_async(dispatch_get_main_queue(), ^{
            [TapTapMoment closeWithTitle:TitleNS content:ContentNS showConfirm:YES];
        });
#endif
    }

    void FetchNotification()
    {
#if TAP_MOMENT_IOS_SDK_AVAILABLE
        dispatch_async(dispatch_get_main_queue(), ^{
            [TapTapMoment fetchNotification];
        });
#endif
    }

    void Publish(const FString& Content, const TArray<FString>& ImagePaths, const TArray<FString>& VideoPaths)
    {
#if TAP_MOMENT_IOS_SDK_AVAILABLE
        NSString* ContentNS = ToNS(Content);
        NSMutableArray<NSString*>* ImagesNS = [NSMutableArray arrayWithCapacity:ImagePaths.Num()];
        for (const FString& ImagePath : ImagePaths)
        {
            [ImagesNS addObject:ToNS(ImagePath)];
        }
        NSString* FirstImageNS = ImagesNS.firstObject;
        NSString* FirstVideoNS = VideoPaths.Num() > 0 ? ToNS(VideoPaths[0]) : nil;
        dispatch_async(dispatch_get_main_queue(), ^{
            if (FirstVideoNS)
            {
                TapTapMomentVideoData* Data = [[TapTapMomentVideoData alloc] init];
                Data.content = ContentNS;
                Data.videos = @[FirstVideoNS];
                if (FirstImageNS) Data.images = @[FirstImageNS];
                [TapTapMoment publish:Data];
            }
            else
            {
                TapTapMomentImageData* Data = [[TapTapMomentImageData alloc] init];
                Data.content = ContentNS;
                if (ImagesNS.count > 0) Data.images = ImagesNS;
                [TapTapMoment publish:Data];
            }
        });
#endif
    }
}

#endif // PLATFORM_IOS

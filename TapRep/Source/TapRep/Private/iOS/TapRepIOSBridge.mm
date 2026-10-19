// Copyright TapTap. All Rights Reserved.
#include "iOS/TapRepIOSBridge.h"

#if PLATFORM_IOS

#import <Foundation/Foundation.h>
#include "TapRepModule.h"
#include "Async/Async.h"
#include "HAL/CriticalSection.h"

#if TAP_REP_IOS_SDK_AVAILABLE
#import <TapTapRepSDK/TapTapRepSDK-Swift.h>
#endif

namespace
{
    NSString* ToNS(const FString& S) { return [NSString stringWithUTF8String:TCHAR_TO_UTF8(*S)]; }
    FString FromNS(NSString* S) { return S ? FString(UTF8_TO_TCHAR([S UTF8String])) : FString(); }

    bool                        GInitialized = false;
}

namespace TapRepIOSBridge
{
    void Init()
    {
        if (GInitialized) { return; }
        GInitialized = true;
        UE_LOG(LogTapRep, Log, TEXT("[TapRep-iOS] Bridge initialized"));
    }

    // openWithOpenUrl:completion: 最终会 present UIViewController，必须切回主线程执行。
    // UE5 iOS 的游戏线程（IOSAppDelegate MainAppThread）不是 UIKit 认定的主线程，
    // 直接在其上做 UIKit/AutoLayout 操作会触发无法定位原因的内部异常崩溃
    // （与 TapRelation/TapMoment 同构问题）。
    void Open(const FString& OpenUrl, FRepCallback Callback)
    {
#if TAP_REP_IOS_SDK_AVAILABLE
        NSString* OpenUrlNS = ToNS(OpenUrl);
        dispatch_async(dispatch_get_main_queue(), ^{
            [TapTapRep openWithOpenUrl:OpenUrlNS completion:^(NSInteger code, NSString * _Nullable message)
            {
                const int32 C=static_cast<int32>(code);const FString M=FromNS(message);AsyncTask(ENamedThreads::GameThread,[Callback,C,M](){if(Callback)Callback(C,M);});
            }];
        });
#else
        UE_LOG(LogTapRep, Warning, TEXT("[TapRep-iOS] SDK not available: TapTapRepSDK.xcframework missing"));
        if(Callback) AsyncTask(ENamedThreads::GameThread,[Callback](){Callback(100003,TEXT("TapTapRepSDK not available"));});
#endif
    }
}

#endif // PLATFORM_IOS

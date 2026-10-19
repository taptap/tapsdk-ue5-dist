// Copyright TapTap. All Rights Reserved.
#include "iOS/TapMomentPlatformIOS.h"

#if PLATFORM_IOS

#include "iOS/TapMomentIOSBridge.h"
#include "TapMomentCallbackManager.h"
#include "TapMomentModule.h"

void FTapMomentPlatformIOS::Initialize(const FTapTapSdkOptions& /*Config*/)
{
    TapMomentIOSBridge::Init([](int32 Code, const FString& Msg)
    {
        FTapMomentCallbackManager::Get().Dispatch(Code, Msg);
    });
}

void FTapMomentPlatformIOS::Open()                             { TapMomentIOSBridge::Open(); }
void FTapMomentPlatformIOS::OpenScene(const FString& SceneId)  { TapMomentIOSBridge::OpenScene(SceneId); }
void FTapMomentPlatformIOS::Close()                            { TapMomentIOSBridge::Close(); }
void FTapMomentPlatformIOS::FetchNotification()                { TapMomentIOSBridge::FetchNotification(); }
void FTapMomentPlatformIOS::SetCallback()                      { /* Delegate 已在 Init 注册，无需再注册 */ }

void FTapMomentPlatformIOS::CloseWithTitle(const FString& Title, const FString& Content)
{
    TapMomentIOSBridge::CloseWithTitle(Title, Content);
}

void FTapMomentPlatformIOS::Publish(const FTapMomentPostData& PostData)
{
    TapMomentIOSBridge::Publish(PostData.Content, PostData.ImagePaths, PostData.VideoPaths);
}

#endif // PLATFORM_IOS

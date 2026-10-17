// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"

#if PLATFORM_IOS

namespace TapMomentIOSBridge
{
    using FMomentCallback = TFunction<void(int32 /*Code*/, const FString& /*Msg*/)>;

    /** 注册持久 Delegate 回调（Init 时调用一次） */
    void Init(FMomentCallback OnCallback);

    void Open();
    void OpenScene(const FString& SceneId);
    void Publish(const FString& Content, const TArray<FString>& ImagePaths, const TArray<FString>& VideoPaths);
    void Close();
    void CloseWithTitle(const FString& Title, const FString& Content);
    void FetchNotification();
}

#endif // PLATFORM_IOS

// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapTapSdkOptions.h"
#include "TapShareTypes.h"

class TAPSHARE_API ITapSharePlatform
{
public:
    virtual ~ITapSharePlatform() = default;

    virtual void Initialize(const FTapTapSdkOptions& Config) = 0;
    virtual void Share(const FTapShareContent& Content) = 0;
    virtual FString GetPlatformName() const = 0;

    // ===== 图片选择（demo 用，默认 no-op；目前仅 Android 实现）=====
    /** 唤起系统图片选择器 */
    virtual void PickImage() {}
    /** 取回已选图片路径（content:// 或本地路径） */
    virtual TArray<FString> GetPickedImagePaths() { return TArray<FString>(); }
    /** 清空已选图片 */
    virtual void ClearPickedImages() {}
};

class TAPSHARE_API FTapSharePlatformFactory
{
public:
    static TSharedPtr<ITapSharePlatform> CreatePlatform();
    static void Reset();
private:
    static TSharedPtr<ITapSharePlatform> PlatformInstance;
};

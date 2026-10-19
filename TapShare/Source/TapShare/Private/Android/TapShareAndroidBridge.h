// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapShareTypes.h"

#if PLATFORM_ANDROID

/**
 * TapShare Android 桥接
 * 路由：GameActivity.shareToTapTap(title, contents, hashtagIds, groupLabelId, failUrl, imagePaths[])
 * 直接构造原生 TapTapShareBuilder 并 share()，同步返回结果码。
 */
namespace TapShareAndroidBridge
{
    /** 分享，返回结果码（0 成功 / -1 未安装 / -2 不支持）*/
    int32 Share(const FTapShareContent& Content);

    /** 唤起系统图片选择器（结果在 GameActivity.onActivityResult 暂存） */
    void PickImage();

    /** 取回已选图片 URI 字符串（content:// 形式） */
    TArray<FString> GetPickedImagePaths();

    /** 清空已选图片 */
    void ClearPickedImages();
}

#endif // PLATFORM_ANDROID

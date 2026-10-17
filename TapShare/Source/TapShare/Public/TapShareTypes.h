// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapShareTypes.generated.h"

/**
 * 分享结果回调（每次 Share 完成广播一次）
 * Code: 0 成功 / -1 未安装 TapTap / -2 版本不支持 / -3 配置错误(iOS)
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShareResult, int32 /*Code*/);

/** 分享结果码（对齐 Android TapTapShareResult / iOS TapTapShare.share completion） */
namespace ETapShareResult
{
    constexpr int32 SUCCESS             = 0;
    constexpr int32 ERROR_NOT_INSTALL   = -1;
#pragma push_macro("ERROR_NOT_SUPPORTED")
#undef ERROR_NOT_SUPPORTED
    constexpr int32 ERROR_NOT_SUPPORTED = -2;
#pragma pop_macro("ERROR_NOT_SUPPORTED")
    constexpr int32 ERROR_CONFIG        = -3; // iOS 专属：appId / shareUrl 缺失
}

/**
 * 分享内容（对齐 Android TapTapShareBuilder + iOS TapTapShareObj）
 */
USTRUCT(BlueprintType)
struct TAPSHARE_API FTapShareContent
{
    GENERATED_BODY()

    /** 分享标题 */
    UPROPERTY(BlueprintReadWrite, Category="TapShare")
    FString Title;

    /** 分享内容（长度需大于 0 且不超过 1MB） */
    UPROPERTY(BlueprintReadWrite, Category="TapShare")
    FString Contents;

    /** 活动和 hashtag id，最多 5 个，多个以英文逗号分隔 */
    UPROPERTY(BlueprintReadWrite, Category="TapShare")
    FString HashtagIds;

    /** 论坛标签 id */
    UPROPERTY(BlueprintReadWrite, Category="TapShare")
    FString GroupLabelId;

    /** 唤起 TapTap 失败时跳转的网页地址（不填则用游戏在 TapTap 的详情页兜底） */
    UPROPERTY(BlueprintReadWrite, Category="TapShare")
    FString FailUrl;

    /** 唤起 TapTap 后跳回当前应用的 universal link（仅 iOS 生效） */
    UPROPERTY(BlueprintReadWrite, Category="TapShare")
    FString BackUrl;

    /** 分享图片本地路径列表（Android 最多 9 张；iOS 读取文件为 Data） */
    UPROPERTY(BlueprintReadWrite, Category="TapShare")
    TArray<FString> ImagePaths;
};

// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "TapMomentTypes.generated.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMomentCallback, int32 /*Code*/, const FString& /*Msg*/);

/** 发布动态数据（对齐 Android PublishMetaData + iOS TapTapMomentPostData） */
USTRUCT(BlueprintType)
struct TAPMOMENT_API FTapMomentPostData
{
    GENERATED_BODY()

    /** 动态文字内容 */
    UPROPERTY(BlueprintReadWrite, Category="TapMoment")
    FString Content;

    /** 图片本地路径列表（iOS 最多取第一个） */
    UPROPERTY(BlueprintReadWrite, Category="TapMoment")
    TArray<FString> ImagePaths;

    /** 视频本地路径列表（iOS 最多取第一个；非空时走 VideoData） */
    UPROPERTY(BlueprintReadWrite, Category="TapMoment")
    TArray<FString> VideoPaths;
};

/** 动态回调 code 常量（对齐 Unity TapTapMomentConstants） */
namespace ETapMomentCallbackCode
{
    constexpr int32 OnStop           = 500;
    constexpr int32 OnResume         = 600;
    constexpr int32 PublishSuccess   = 10000;
    constexpr int32 PublishFail      = 10100;
    constexpr int32 PublishCancel    = 10200;
    constexpr int32 GetNoticeSuccess = 20000;
    constexpr int32 GetNoticeFail    = 20100;
    constexpr int32 MomentAppear     = 30000;
    constexpr int32 MomentDisappear  = 30100;
    constexpr int32 InitSuccess      = 40000;
    constexpr int32 InitFail         = 40100;
    constexpr int32 CloseCancel      = 50000;
    constexpr int32 CloseConfirm     = 50100;
    constexpr int32 LoginSuccess     = 60000;
}

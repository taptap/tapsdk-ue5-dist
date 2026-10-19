// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapShareTypes.h"
#include "TapShare.generated.h"

/**
 * TapShare 门面（Android + iOS，其它平台 no-op）
 * 功能参考原生：Android TapTapShareBuilder.share / iOS TapTapShare.share
 */
UCLASS()
class TAPSHARE_API UTapShare : public UObject
{
    GENERATED_BODY()
public:
    /** 分享结果回调（每次 Share 完成广播一次）*/
    static FOnShareResult OnShareResult;

    /**
     * 分享到 TapTap（Android + iOS，其它平台 no-op）
     * 结果通过 OnShareResult 广播：Code=0 成功，其它为错误码
     * @param Content 分享内容
     */
    UFUNCTION(BlueprintCallable, Category="TapShare")
    static void Share(const FTapShareContent& Content);

    /**
     * UE Extension：唤起系统图片选择器（demo 用，目前仅 Android），不属于跨端统一业务 API。
     * 选择结果暂存于原生层，经 GetPickedImagePaths 取回填入 FTapShareContent.ImagePaths
     */
    UFUNCTION(BlueprintCallable, Category="TapShare")
    static void PickImage();

    /** UE Extension：取回已选图片路径（content:// 或本地路径；非 Android 返回空）。 */
    UFUNCTION(BlueprintCallable, Category="TapShare")
    static TArray<FString> GetPickedImagePaths();

    /** UE Extension：清空已选图片。 */
    UFUNCTION(BlueprintCallable, Category="TapShare")
    static void ClearPickedImages();
};

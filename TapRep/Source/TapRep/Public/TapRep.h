// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapRepTypes.h"
#include "TapRep.generated.h"
DECLARE_DELEGATE_TwoParams(FTapRepCallback, int32, const FString&);

/**
 * TapRep 门面（Android + iOS，其它平台 no-op）
 * 对齐 Unity TapTapRep.Open(openUrl, Action<int, string>)
 */
UCLASS()
class TAPREP_API UTapRep : public UObject
{
    GENERATED_BODY()
public:
    /**
     * 打开指定 URL（Android + iOS，其它平台 no-op）
     * 结果只交付给本次调用传入的 Callback：Code=0 成功，其它为错误码。
     * @param OpenUrl 要打开的 URL 地址（为空则立即执行 Callback 并返回 PARAM_ERROR）
     * C++ 原生单播 Delegate；SDK 保存副本，CreateUObject / CreateWeakLambda 不为接收对象保活。
     * @param Callback 本次调用的结果回调
     */
    static void Open(const FString& OpenUrl, const FTapRepCallback& Callback);
};

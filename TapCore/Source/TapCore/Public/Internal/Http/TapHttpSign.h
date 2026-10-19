// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"

/**
 * 对齐 Unity TapHttpSign 的 ITapHttpSign.HandleData (传 sign 算法所需上下文)
 */
struct TAPCORE_API FTapHttpSignData
{
    FString Url;
    FString Method;
    bool bEnableAuthorization = false;
    FString Body;
    TMap<FString, FString>& Headers;  // in/out
};

/**
 * 签名策略接口. 对齐 Unity TapHttpSign.cs ITapHttpSign / Android TapHttpSign.kt ITapHttpSign.
 */
class TAPCORE_API ITapHttpSign
{
public:
    virtual ~ITapHttpSign() = default;

    /** 公共头 (X-Tap-PN/Device-Id/Platform/Module/Ts/Nonce/Lang/UA). PUT/DELETE 等返回空. */
    virtual TMap<FString, FString> GetFixedHeaders(
        const FString& Url, const FString& Method,
        const FString& ModuleName, const FString& ModuleVersion,
        bool bEnableAuthorization) = 0;

    /** 固定 query 参数 (默认 client_id). */
    virtual TMap<FString, FString> GetFixedQueryParams() = 0;

    /** 计算并写入 X-Tap-Sign. 依赖 Data.Headers 里已有的 X-Tap-* + Body. */
    virtual void Sign(FTapHttpSignData& Data) = 0;
};

/**
 * Factory. 对齐 Unity TapHttpSign.CreateDefaultSign / CreateNoneSign.
 */
class TAPCORE_API FTapHttpSign
{
public:
    /** 默认实现: 内部委托 FTapHttpCommonParams::GetCommonHeaders/AddSignature, 算法不重写 */
    static TSharedRef<ITapHttpSign> CreateDefaultSign();

    /** None: 三个方法都返回空, sign 不动 Headers. 用于不需要签名的端点. */
    static TSharedRef<ITapHttpSign> CreateNoneSign();
};

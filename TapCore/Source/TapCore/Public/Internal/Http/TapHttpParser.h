// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Internal/Http/TapHttpResult.h"  // FTapHttpJsonResult
#include "Templates/SharedPointer.h"

/**
 * 响应解析接口. 对齐 Unity TapHttpParser.cs ITapHttpParser / Android TapHttpParser.kt.
 */
class TAPCORE_API ITapHttpParser
{
public:
    virtual ~ITapHttpParser() = default;

    /**
     * @param StatusCode    HTTP status
     * @param Content       响应体字符串
     * @param bHttpOk       transport 层是否成功 (false = 网络层失败)
     * @param ErrorMessage  transport 层错误 (仅 bHttpOk=false 时使用)
     */
    virtual FTapHttpJsonResult Parse(
        int32 StatusCode, const FString& Content,
        bool bHttpOk, const FString& ErrorMessage) = 0;
};

class TAPCORE_API FTapHttpParser
{
public:
    /** TapSDK envelope (success/now/data + error) 解析器. 对齐 Unity TapHttpParser.Default. */
    static TSharedRef<ITapHttpParser> CreateEnvelopeParser();

    /** 保留数字原文，供 int64 字段精确读取，避免先转 double 丢失精度。 */
    static TSharedRef<ITapHttpParser> CreateEnvelopeParser(bool bStoreNumbersAsStrings);

    // 未来如需 EventParser (TapDB 场景), 加 CreateEventParser() 即可.
};

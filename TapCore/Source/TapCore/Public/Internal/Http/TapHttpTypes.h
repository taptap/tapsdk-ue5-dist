// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "TapHttpTypes.generated.h"

/**
 * HTTP 请求方法枚举
 */
UENUM(BlueprintType)
enum class ETapHttpMethod : uint8
{
	Get     UMETA(DisplayName = "GET"),
	Post    UMETA(DisplayName = "POST"),
	Put     UMETA(DisplayName = "PUT"),
	Delete  UMETA(DisplayName = "DELETE")
};

/**
 * HTTP 错误类型枚举
 */
UENUM(BlueprintType)
enum class ETapHttpErrorType : uint8
{
	None            UMETA(DisplayName = "None"),
	NetworkError    UMETA(DisplayName = "Network Error"),
	Timeout         UMETA(DisplayName = "Timeout"),
	InvalidResponse UMETA(DisplayName = "Invalid Response"),
	ServerError     UMETA(DisplayName = "Server Error"),
	Unknown         UMETA(DisplayName = "Unknown")
};

/**
 * 服务端错误数据
 */
USTRUCT(BlueprintType)
struct TAPCORE_API FTapHttpErrorData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TapSDK|Http")
	int32 Code = 0;

	UPROPERTY(BlueprintReadOnly, Category = "TapSDK|Http")
	FString Error;

	UPROPERTY(BlueprintReadOnly, Category = "TapSDK|Http")
	FString Msg;

	UPROPERTY(BlueprintReadOnly, Category = "TapSDK|Http")
	FString ErrorDescription;

	bool IsValid() const { return !Error.IsEmpty(); }

	void Reset()
	{
		Code = 0;
		Error.Empty();
		Msg.Empty();
		ErrorDescription.Empty();
	}
};

/**
 * 服务端响应结构
 */
USTRUCT(BlueprintType)
struct TAPCORE_API FTapHttpServerResponse
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "TapSDK|Http")
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "TapSDK|Http")
	int64 Now = 0;  // 服务器时间戳

	TSharedPtr<FJsonObject> Data;

	FTapHttpErrorData ErrorData;

	bool IsSuccess() const { return bSuccess && !ErrorData.IsValid(); }

	void Reset()
	{
		bSuccess = false;
		Now = 0;
		Data.Reset();
		ErrorData.Reset();
	}
};

/**
 * HTTP 方法辅助函数
 */
namespace TapHttpMethodHelper
{
	inline FString ToString(ETapHttpMethod Method)
	{
		switch (Method)
		{
		case ETapHttpMethod::Get:    return TEXT("GET");
		case ETapHttpMethod::Post:   return TEXT("POST");
		case ETapHttpMethod::Put:    return TEXT("PUT");
		case ETapHttpMethod::Delete: return TEXT("DELETE");
		default:                     return TEXT("GET");
		}
	}

	inline ETapHttpMethod FromString(const FString& MethodStr)
	{
		if (MethodStr.Equals(TEXT("POST"), ESearchCase::IgnoreCase))
		{
			return ETapHttpMethod::Post;
		}
		if (MethodStr.Equals(TEXT("PUT"), ESearchCase::IgnoreCase))
		{
			return ETapHttpMethod::Put;
		}
		if (MethodStr.Equals(TEXT("DELETE"), ESearchCase::IgnoreCase))
		{
			return ETapHttpMethod::Delete;
		}
		return ETapHttpMethod::Get;
	}
}

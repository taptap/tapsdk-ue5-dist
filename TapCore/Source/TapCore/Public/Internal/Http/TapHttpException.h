// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapHttpTypes.h"

/**
 * TapSDK HTTP 异常基类
 * 对齐 Unity 的 AbsTapHttpException
 */
class TAPCORE_API FTapHttpException : public TSharedFromThis<FTapHttpException>
{
public:
	FTapHttpException(const FString& InMessage)
		: Message(InMessage)
		, ErrorType(ETapHttpErrorType::Unknown)
	{}

	virtual ~FTapHttpException() = default;

	/** 获取错误消息 */
	FString GetMessage() const { return Message; }

	/** 获取错误类型 */
	ETapHttpErrorType GetErrorType() const { return ErrorType; }

	/** 获取类型名称（用于调试） */
	virtual FString GetTypeName() const { return TEXT("FTapHttpException"); }

protected:
	FString Message;
	ETapHttpErrorType ErrorType;
};

/**
 * 未知错误异常
 */
class TAPCORE_API FTapHttpUnknownException : public FTapHttpException
{
public:
	FTapHttpUnknownException(const FString& InMessage = TEXT("Unknown error"))
		: FTapHttpException(InMessage)
	{
		ErrorType = ETapHttpErrorType::Unknown;
	}

	virtual FString GetTypeName() const override { return TEXT("FTapHttpUnknownException"); }
};

/**
 * 网络错误异常（无网络连接）
 * 对齐 Unity 的 TapHttpNetworkErrorException
 */
class TAPCORE_API FTapHttpNetworkErrorException : public FTapHttpException
{
public:
	FTapHttpNetworkErrorException(const FString& InMessage = TEXT("Network error"))
		: FTapHttpException(InMessage)
	{
		ErrorType = ETapHttpErrorType::NetworkError;
	}

	virtual FString GetTypeName() const override { return TEXT("FTapHttpNetworkErrorException"); }
};

/**
 * 超时异常
 */
class TAPCORE_API FTapHttpTimeoutException : public FTapHttpException
{
public:
	FTapHttpTimeoutException(const FString& InMessage = TEXT("Request timeout"))
		: FTapHttpException(InMessage)
	{
		ErrorType = ETapHttpErrorType::Timeout;
	}

	virtual FString GetTypeName() const override { return TEXT("FTapHttpTimeoutException"); }
};

/**
 * 无效响应异常（无法解析）
 * 对齐 Unity 的 TapHttpInvalidResponseException
 */
class TAPCORE_API FTapHttpInvalidResponseException : public FTapHttpException
{
public:
	FTapHttpInvalidResponseException(int32 InStatusCode, const FString& InMessage)
		: FTapHttpException(InMessage)
		, StatusCode(InStatusCode)
	{
		ErrorType = ETapHttpErrorType::InvalidResponse;
	}

	int32 GetStatusCode() const { return StatusCode; }

	virtual FString GetTypeName() const override { return TEXT("FTapHttpInvalidResponseException"); }

private:
	int32 StatusCode;
};

/**
 * 服务端业务错误异常
 * 对齐 Unity 的 TapHttpServerException
 */
class TAPCORE_API FTapHttpServerException : public FTapHttpException
{
public:
	FTapHttpServerException(
		int32 InStatusCode,
		const FTapHttpServerResponse& InResponse,
		const FTapHttpErrorData& InErrorData
	)
		: FTapHttpException(InErrorData.Msg)
		, StatusCode(InStatusCode)
		, Response(InResponse)
		, ErrorData(InErrorData)
	{
		ErrorType = ETapHttpErrorType::ServerError;
	}

	int32 GetStatusCode() const { return StatusCode; }
	const FTapHttpServerResponse& GetResponse() const { return Response; }
	const FTapHttpErrorData& GetErrorData() const { return ErrorData; }

	virtual FString GetTypeName() const override { return TEXT("FTapHttpServerException"); }

private:
	int32 StatusCode;
	FTapHttpServerResponse Response;
	FTapHttpErrorData ErrorData;
};

/**
 * 异常工厂辅助函数
 */
namespace TapHttpExceptionFactory
{
	inline TSharedPtr<FTapHttpException> CreateNetworkError(const FString& Message = TEXT("Network error"))
	{
		return MakeShared<FTapHttpNetworkErrorException>(Message);
	}

	inline TSharedPtr<FTapHttpException> CreateTimeout(const FString& Message = TEXT("Request timeout"))
	{
		return MakeShared<FTapHttpTimeoutException>(Message);
	}

	inline TSharedPtr<FTapHttpException> CreateInvalidResponse(int32 StatusCode, const FString& Message)
	{
		return MakeShared<FTapHttpInvalidResponseException>(StatusCode, Message);
	}

	inline TSharedPtr<FTapHttpException> CreateServerError(
		int32 StatusCode,
		const FTapHttpServerResponse& Response,
		const FTapHttpErrorData& ErrorData)
	{
		return MakeShared<FTapHttpServerException>(StatusCode, Response, ErrorData);
	}

	inline TSharedPtr<FTapHttpException> CreateUnknown(const FString& Message = TEXT("Unknown error"))
	{
		return MakeShared<FTapHttpUnknownException>(Message);
	}
}

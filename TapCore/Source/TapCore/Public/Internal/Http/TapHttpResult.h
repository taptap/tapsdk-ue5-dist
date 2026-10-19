// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapHttpException.h"

/**
 * HTTP 请求结果包装类（模板）
 * 对齐 Unity 的 TapHttpResult<T>
 */
template<typename T>
class FTapHttpResult
{
public:
	FTapHttpResult() : bSuccess(false) {}

	/** 是否成功 */
	bool IsSuccess() const { return bSuccess; }

	/** 获取数据（const 版本） */
	const T& GetData() const { return Data; }

	/** 获取数据（可修改版本） */
	T& GetData() { return Data; }

	/** 获取异常 */
	TSharedPtr<FTapHttpException> GetException() const { return Exception; }

	/** 创建成功结果 */
	static FTapHttpResult Success(const T& InData)
	{
		FTapHttpResult Result;
		Result.bSuccess = true;
		Result.Data = InData;
		return Result;
	}

	/** 创建成功结果（移动语义） */
	static FTapHttpResult Success(T&& InData)
	{
		FTapHttpResult Result;
		Result.bSuccess = true;
		Result.Data = MoveTemp(InData);
		return Result;
	}

	/** 创建网络错误结果 */
	static FTapHttpResult NetworkError(const FString& Message = TEXT("Network error"))
	{
		FTapHttpResult Result;
		Result.bSuccess = false;
		Result.Exception = TapHttpExceptionFactory::CreateNetworkError(Message);
		return Result;
	}

	/** 创建超时错误结果 */
	static FTapHttpResult TimeoutError(const FString& Message = TEXT("Request timeout"))
	{
		FTapHttpResult Result;
		Result.bSuccess = false;
		Result.Exception = TapHttpExceptionFactory::CreateTimeout(Message);
		return Result;
	}

	/** 创建无效响应错误结果 */
	static FTapHttpResult InvalidResponse(int32 StatusCode, const FString& Message)
	{
		FTapHttpResult Result;
		Result.bSuccess = false;
		Result.Exception = TapHttpExceptionFactory::CreateInvalidResponse(StatusCode, Message);
		return Result;
	}

	/** 创建服务端错误结果 */
	static FTapHttpResult ServerError(
		int32 StatusCode,
		const FTapHttpServerResponse& Response,
		const FTapHttpErrorData& ErrorData)
	{
		FTapHttpResult Result;
		Result.bSuccess = false;
		Result.Exception = TapHttpExceptionFactory::CreateServerError(StatusCode, Response, ErrorData);
		return Result;
	}

	/** 创建未知错误结果 */
	static FTapHttpResult UnknownError(const FString& Message = TEXT("Unknown error"))
	{
		FTapHttpResult Result;
		Result.bSuccess = false;
		Result.Exception = TapHttpExceptionFactory::CreateUnknown(Message);
		return Result;
	}

	/** 从异常创建失败结果 */
	static FTapHttpResult FromException(TSharedPtr<FTapHttpException> InException)
	{
		FTapHttpResult Result;
		Result.bSuccess = false;
		Result.Exception = InException;
		return Result;
	}

private:
	bool bSuccess;
	T Data;
	TSharedPtr<FTapHttpException> Exception;
};

/**
 * 常用类型别名
 */
using FTapHttpJsonResult = FTapHttpResult<TSharedPtr<FJsonObject>>;
using FTapHttpStringResult = FTapHttpResult<FString>;

/**
 * JSON 结果完成委托
 */
DECLARE_DELEGATE_OneParam(FTapHttpJsonResultDelegate, const FTapHttpJsonResult&);

/**
 * 字符串结果完成委托
 */
DECLARE_DELEGATE_OneParam(FTapHttpStringResultDelegate, const FTapHttpStringResult&);

// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttpRetryStrategy.h"
#include "Internal/Http/TapHttpTime.h"
#include "Internal/Http/TapHttpErrorConstants.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapHttpRetry, Log, All);

// ============== 后退策略实现 ==============

/**
 * 固定间隔后退策略
 */
class FFixedBackoffStrategy : public ITapHttpBackoffStrategy
{
public:
	FFixedBackoffStrategy(int32 InMaxCount, int64 InIntervalMillis)
		: MaxCount(InMaxCount)
		, IntervalMillis(InIntervalMillis)
		, CurrentCount(0)
		, bCanTimeDeltaRetry(true)
	{}

	virtual int64 NextBackoffMillis() override
	{
		if (++CurrentCount < MaxCount)
		{
			UE_LOG(LogTapHttpRetry, Verbose, TEXT("Fixed backoff: retry %d/%d, interval %lld ms"),
				CurrentCount, MaxCount, IntervalMillis);
			return IntervalMillis;
		}
		UE_LOG(LogTapHttpRetry, Log, TEXT("Fixed backoff: max retries reached (%d)"), MaxCount);
		return -1; // 达到最大重试次数
	}

	virtual bool CanInvalidTimeRetry() override
	{
		if (bCanTimeDeltaRetry)
		{
			bCanTimeDeltaRetry = false;
			return true;
		}
		return false;
	}

	virtual void Reset() override
	{
		CurrentCount = 0;
		bCanTimeDeltaRetry = true;
	}

private:
	int32 MaxCount;
	int64 IntervalMillis;
	int32 CurrentCount;
	bool bCanTimeDeltaRetry;
};

/**
 * 指数退避策略
 */
class FExponentialBackoffStrategy : public ITapHttpBackoffStrategy
{
public:
	FExponentialBackoffStrategy(int64 InInitialMillis, int64 InMaxMillis, int32 InMultiplier)
		: InitialIntervalMillis(InInitialMillis)
		, MaxIntervalMillis(InMaxMillis)
		, Multiplier(InMultiplier)
		, CurrentIntervalMillis(InInitialMillis)
		, bCanTimeDeltaRetry(true)
	{}

	virtual int64 NextBackoffMillis() override
	{
		int64 ReturnInterval = CurrentIntervalMillis;

		// 计算下一次间隔
		int64 NextInterval = CurrentIntervalMillis * Multiplier;
		if (NextInterval > MaxIntervalMillis)
		{
			CurrentIntervalMillis = MaxIntervalMillis;
		}
		else
		{
			CurrentIntervalMillis = NextInterval;
		}

		UE_LOG(LogTapHttpRetry, Verbose, TEXT("Exponential backoff: interval %lld ms, next %lld ms"),
			ReturnInterval, CurrentIntervalMillis);

		return ReturnInterval;
	}

	virtual bool CanInvalidTimeRetry() override
	{
		if (bCanTimeDeltaRetry)
		{
			bCanTimeDeltaRetry = false;
			return true;
		}
		return false;
	}

	virtual void Reset() override
	{
		CurrentIntervalMillis = InitialIntervalMillis;
		bCanTimeDeltaRetry = true;
	}

private:
	int64 InitialIntervalMillis;
	int64 MaxIntervalMillis;
	int32 Multiplier;
	int64 CurrentIntervalMillis;
	bool bCanTimeDeltaRetry;
};

/**
 * 不后退策略
 */
class FNoneBackoffStrategy : public ITapHttpBackoffStrategy
{
public:
	virtual int64 NextBackoffMillis() override { return -1; }
	virtual bool CanInvalidTimeRetry() override { return false; }
	virtual void Reset() override {}
};

// ============== 重试策略实现 ==============

/**
 * 默认重试策略
 */
class FDefaultRetryStrategy : public ITapHttpRetryStrategy
{
public:
	FDefaultRetryStrategy(TSharedPtr<ITapHttpBackoffStrategy> InBackoffStrategy)
		: BackoffStrategy(InBackoffStrategy)
	{}

	virtual int64 NextRetryMillis(TSharedPtr<FTapHttpException> Exception) override
	{
		if (!Exception.IsValid())
		{
			return -1;
		}

		// 处理服务端错误
		if (Exception->GetErrorType() == ETapHttpErrorType::ServerError)
		{
			auto ServerException = StaticCastSharedPtr<FTapHttpServerException>(Exception);
			if (ServerException.IsValid())
			{
				int32 StatusCode = ServerException->GetStatusCode();

				// 5xx 服务器错误，按后退策略重试
				if (StatusCode >= 500 && StatusCode <= 599)
				{
					UE_LOG(LogTapHttpRetry, Log, TEXT("Server error %d, will retry"), StatusCode);
					return BackoffStrategy->NextBackoffMillis();
				}

				// 检查特定错误类型
				const FTapHttpErrorData& ErrorData = ServerException->GetErrorData();

				// invalid_time 错误：修复时间后立即重试（仅一次）
				if (ErrorData.Error == FTapHttpErrorConstants::ERROR_INVALID_TIME)
				{
					UE_LOG(LogTapHttpRetry, Log, TEXT("Invalid time error, fixing time and retrying"));
					FTapHttpTime::FixTime(ServerException->GetResponse().Now);
					if (BackoffStrategy->CanInvalidTimeRetry())
					{
						return 0; // 立即重试
					}
				}

				// server_error 错误：按后退策略重试
				if (ErrorData.Error == FTapHttpErrorConstants::ERROR_SERVER_ERROR)
				{
					UE_LOG(LogTapHttpRetry, Log, TEXT("Server error, will retry"));
					return BackoffStrategy->NextBackoffMillis();
				}
			}
		}

		// 无效响应且状态码为 5xx
		if (Exception->GetErrorType() == ETapHttpErrorType::InvalidResponse)
		{
			auto InvalidException = StaticCastSharedPtr<FTapHttpInvalidResponseException>(Exception);
			if (InvalidException.IsValid())
			{
				int32 StatusCode = InvalidException->GetStatusCode();
				if (StatusCode >= 500 && StatusCode <= 599)
				{
					UE_LOG(LogTapHttpRetry, Log, TEXT("Invalid response with status %d, will retry"), StatusCode);
					return BackoffStrategy->NextBackoffMillis();
				}
			}
		}

		// 网络错误（无网络连接），按后退策略重试
		if (Exception->GetErrorType() == ETapHttpErrorType::NetworkError)
		{
			UE_LOG(LogTapHttpRetry, Log, TEXT("Network error, will retry"));
			return BackoffStrategy->NextBackoffMillis();
		}

		// 请求超时，按后退策略重试
		if (Exception->GetErrorType() == ETapHttpErrorType::Timeout)
		{
			UE_LOG(LogTapHttpRetry, Log, TEXT("Request timeout, will retry"));
			return BackoffStrategy->NextBackoffMillis();
		}

		return -1; // 不重试
	}

	virtual void Reset() override
	{
		if (BackoffStrategy.IsValid())
		{
			BackoffStrategy->Reset();
		}
	}

private:
	TSharedPtr<ITapHttpBackoffStrategy> BackoffStrategy;
};

/**
 * 不重试策略
 */
class FNoneRetryStrategy : public ITapHttpRetryStrategy
{
public:
	virtual int64 NextRetryMillis(TSharedPtr<FTapHttpException> Exception) override
	{
		return -1;
	}

	virtual void Reset() override {}
};

// ============== 工厂方法 ==============

TSharedPtr<ITapHttpBackoffStrategy> FTapHttpBackoffStrategy::CreateFixed(int32 MaxCount, int64 IntervalMillis)
{
	return MakeShared<FFixedBackoffStrategy>(MaxCount, IntervalMillis);
}

TSharedPtr<ITapHttpBackoffStrategy> FTapHttpBackoffStrategy::CreateExponential(
	int64 InitialIntervalMillis, int64 MaxIntervalMillis, int32 Multiplier)
{
	return MakeShared<FExponentialBackoffStrategy>(InitialIntervalMillis, MaxIntervalMillis, Multiplier);
}

TSharedPtr<ITapHttpBackoffStrategy> FTapHttpBackoffStrategy::CreateNone()
{
	return MakeShared<FNoneBackoffStrategy>();
}

TSharedPtr<ITapHttpRetryStrategy> FTapHttpRetryStrategy::CreateDefault(
	TSharedPtr<ITapHttpBackoffStrategy> BackoffStrategy)
{
	return MakeShared<FDefaultRetryStrategy>(BackoffStrategy);
}

TSharedPtr<ITapHttpRetryStrategy> FTapHttpRetryStrategy::CreateNone()
{
	return MakeShared<FNoneRetryStrategy>();
}

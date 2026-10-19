// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapHttpException.h"

/**
 * 后退策略接口
 * 对齐 Unity 的 ITapHttpBackoffStrategy
 */
class TAPCORE_API ITapHttpBackoffStrategy
{
public:
	virtual ~ITapHttpBackoffStrategy() = default;

	/**
	 * 获取下一个后退时间（毫秒）
	 * @return 后退时间毫秒数，-1 表示不再重试
	 */
	virtual int64 NextBackoffMillis() = 0;

	/**
	 * 判断是否可以重试时间校准错误
	 * @return 是否可以重试
	 */
	virtual bool CanInvalidTimeRetry() = 0;

	/**
	 * 重置策略状态
	 */
	virtual void Reset() = 0;
};

/**
 * 重试策略接口
 * 对齐 Unity 的 ITapHttpRetryStrategy
 */
class TAPCORE_API ITapHttpRetryStrategy
{
public:
	virtual ~ITapHttpRetryStrategy() = default;

	/**
	 * 获取下次重试的时间（毫秒）
	 * @param Exception 异常信息
	 * @return 重试间隔毫秒数，-1 表示不重试
	 */
	virtual int64 NextRetryMillis(TSharedPtr<FTapHttpException> Exception) = 0;

	/**
	 * 重置策略状态
	 */
	virtual void Reset() = 0;
};

/**
 * 后退策略工厂
 */
class TAPCORE_API FTapHttpBackoffStrategy
{
public:
	/**
	 * 创建固定重试策略
	 * @param MaxCount 最大重试次数
	 * @param IntervalMillis 重试间隔（毫秒），默认 100ms
	 */
	static TSharedPtr<ITapHttpBackoffStrategy> CreateFixed(
		int32 MaxCount,
		int64 IntervalMillis = 100
	);

	/**
	 * 创建指数退避策略
	 * @param InitialIntervalMillis 初始间隔（毫秒），默认 2000ms
	 * @param MaxIntervalMillis 最大间隔（毫秒），默认 600000ms (10分钟)
	 * @param Multiplier 倍数，默认 2
	 */
	static TSharedPtr<ITapHttpBackoffStrategy> CreateExponential(
		int64 InitialIntervalMillis = 2000,
		int64 MaxIntervalMillis = 600000,
		int32 Multiplier = 2
	);

	/**
	 * 创建不后退策略
	 */
	static TSharedPtr<ITapHttpBackoffStrategy> CreateNone();
};

/**
 * 重试策略工厂
 */
class TAPCORE_API FTapHttpRetryStrategy
{
public:
	/**
	 * 创建默认重试策略
	 * @param BackoffStrategy 后退策略
	 */
	static TSharedPtr<ITapHttpRetryStrategy> CreateDefault(
		TSharedPtr<ITapHttpBackoffStrategy> BackoffStrategy
	);

	/**
	 * 创建不重试策略
	 */
	static TSharedPtr<ITapHttpRetryStrategy> CreateNone();
};

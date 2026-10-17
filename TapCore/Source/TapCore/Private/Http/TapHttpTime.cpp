// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttpTime.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapHttpTime, Log, All);

bool  FTapHttpTime::bIsServerTime = false;
int64 FTapHttpTime::TimeDiffMs    = 0;

int64 FTapHttpTime::GetMonotonicMs()
{
	// FPlatformTime::Seconds 在所有桌面平台都是单调高精度计数 (Windows QPC / Mac mach_absolute_time / Linux CLOCK_MONOTONIC)
	return static_cast<int64>(FPlatformTime::Seconds() * 1000.0);
}

int64 FTapHttpTime::GetCurrentTime()
{
	if (bIsServerTime)
	{
		// 对齐 Android: timeDiff + elapsedRealtime, 再 /1000 转秒
		return (TimeDiffMs + GetMonotonicMs()) / 1000;
	}
	// 未校准: 用 wall clock 当初值, 等首个响应回来再切换到单调时钟
	return FDateTime::UtcNow().ToUnixTimestamp();
}

void FTapHttpTime::FixTime(int64 ServerTime)
{
	if (ServerTime <= 0)
	{
		return;
	}

	// 对齐 Android calibrateServerTime: timeDiff = serverNow*1000 - elapsedRealtime
	const int64 NewDiffMs = ServerTime * 1000LL - GetMonotonicMs();

	if (!bIsServerTime || NewDiffMs != TimeDiffMs)
	{
		TimeDiffMs    = NewDiffMs;
		bIsServerTime = true;
		UE_LOG(LogTapHttpTime, Log, TEXT("Time calibrated: server=%lld, timeDiffMs=%lld"),
			ServerTime, TimeDiffMs);
	}
}

int64 FTapHttpTime::GetCurrentServerTime()
{
	return bIsServerTime ? GetCurrentTime() : 0;
}

void FTapHttpTime::ResetLastServerTime(int64 ServerTime)
{
	FixTime(ServerTime);
}

bool FTapHttpTime::IsTimeSynced()
{
	return bIsServerTime;
}

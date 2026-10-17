// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS && (PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX)

#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"

/**
 * 自动化测试通用异步等待：当 *bDoneFlag 变 true，或超时（秒），latent command 返回 true 让框架继续。
 * 没有自带 Result，业务断言由具体测试在回调里完成。
 */
class FWaitForFlagLatentCommand : public IAutomationLatentCommand
{
public:
	FWaitForFlagLatentCommand(bool* InFlag, FAutomationTestBase* InTest,
	                          const TCHAR* InLabel, double InTimeoutSec = 20.0)
		: Flag(InFlag), Test(InTest), Label(InLabel),
		  TimeoutSec(InTimeoutSec), StartTime(FPlatformTime::Seconds()) {}

	virtual bool Update() override
	{
		if (Flag && *Flag) return true;
		if ((FPlatformTime::Seconds() - StartTime) > TimeoutSec)
		{
			if (Test)
			{
				Test->AddError(FString::Printf(
					TEXT("[%s] Timed out after %.1fs waiting for async callback"),
					Label, TimeoutSec));
			}
			return true;
		}
		return false;
	}

private:
	bool* Flag;
	FAutomationTestBase* Test;
	const TCHAR* Label;
	double TimeoutSec;
	double StartTime;
};

#endif // WITH_DEV_AUTOMATION_TESTS

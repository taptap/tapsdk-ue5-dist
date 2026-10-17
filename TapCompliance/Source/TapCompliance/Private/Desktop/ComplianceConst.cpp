// Copyright TapTap. All Rights Reserved.

#include "ComplianceConst.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"

namespace TapComplianceConst
{
	namespace
	{
		/** 进程级 Host 覆盖；空串表示按内部 RND 开关自动选 (RND/PROD) */
		FString GHostOverride;
	}

	const FString& GetHostCN()
	{
		// 优先级: 显式 Override > 内部 RND 开关自动切换 > PROD 默认.
		// 之前默认只返回 PROD, 导致 Init Config 配 bIsRND=true 时 TapLogin 走 RND
		// (open.api.xdrnd.cn / accounts.xdrnd.cn) 拿到 RND token, 但 TapCompliance
		// 仍打到 PROD (tapsdk.tapapis.cn), 服务端校验报 "invalid self-contained access token".
		if (!GHostOverride.IsEmpty())
		{
			return GHostOverride;
		}
		return FTapSdkConfigState::IsRNDEnvironment() ? HOST_CN_RND : HOST_CN_PROD;
	}

	void SetHostOverride(const FString& Host)
	{
		GHostOverride = Host;
	}

#if WITH_DEV_AUTOMATION_TESTS
	const FString& GetHostOverride()
	{
		return GHostOverride;
	}
#endif
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

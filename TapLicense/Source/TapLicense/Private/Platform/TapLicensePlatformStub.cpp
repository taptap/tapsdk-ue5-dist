// Copyright TapTap. All Rights Reserved.
#include "Platform/TapLicensePlatformStub.h"
#include "CoreMinimal.h"

#if PLATFORM_MAC || PLATFORM_LINUX

#include "TapLicenseModule.h"

void FTapLicensePlatformStub::Initialize()
{
	UE_LOG(LogTapLicense, Warning,
		TEXT("TapLicense: 正版验证仅 Windows 支持，当前平台为 no-op stub。"));
}

#endif // PLATFORM_MAC || PLATFORM_LINUX

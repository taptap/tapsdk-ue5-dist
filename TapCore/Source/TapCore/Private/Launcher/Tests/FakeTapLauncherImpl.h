// Copyright TapTap. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Launcher/TapLauncherTypes.h"

/**
 * Configuration for test behavior of the launcher stub.
 * Tests set fields here before calling UTapLauncher APIs.
 */
struct FFakeLauncherBehavior
{
	bool   bCheckPasses    = true;
	FString FakeOpenId     = TEXT("fake_openid_123");
	ETapLauncherInitResult FailCode = ETapLauncherInitResult::NoPlatform;
	FString FailMsg        = TEXT("fake error");
	bool   bLicenseOwned   = true;
	TMap<FString, bool> DLCOwnership; // skuId -> owned
};

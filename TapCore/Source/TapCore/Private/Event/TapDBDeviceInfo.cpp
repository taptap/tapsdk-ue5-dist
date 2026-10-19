// Copyright TapTap. All Rights Reserved.

#include "TapDBDeviceInfo.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Internal/TapDBIdentity.h"
#include "Internal/TapDBConstants.h"
#include "TapCoreModule.h"
#include "Misc/Guid.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProcess.h"

namespace
{
	FString ResolveGameVersion(const FString& ConfiguredGameVersion)
	{
		if (!ConfiguredGameVersion.IsEmpty())
		{
			return ConfiguredGameVersion;
		}

		FString ProjectVersion;
		if (GConfig)
		{
			GConfig->GetString(
				TEXT("/Script/EngineSettings.GeneralProjectSettings"),
				TEXT("ProjectVersion"),
				ProjectVersion,
				GGameIni);
		}

		return ProjectVersion.IsEmpty() ? FString(FApp::GetBuildVersion()) : ProjectVersion;
	}
}

TSharedPtr<FJsonObject> FTapDBDeviceInfo::CollectBasicProperties(
	const FTapDBIdentity& Identity,
	const FString& GameVersion,
	const FString& Channel,
	const FString& SDKVersion)
{
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();

	// OS name
#if PLATFORM_WINDOWS
	Props->SetStringField(TapDBConstants::PROP_OS, TEXT("Windows"));
#elif PLATFORM_MAC
	Props->SetStringField(TapDBConstants::PROP_OS, TEXT("Mac"));
#elif PLATFORM_LINUX
	Props->SetStringField(TapDBConstants::PROP_OS, TEXT("Linux"));
#else
	Props->SetStringField(TapDBConstants::PROP_OS, TEXT("Unknown"));
#endif

	// Device model
	Props->SetStringField(TapDBConstants::PROP_MODEL, FPlatformMisc::GetDefaultDeviceProfileName());

	// OS version
	Props->SetStringField(TapDBConstants::PROP_OS_VERSION, FPlatformMisc::GetOSVersion());

	// os_type: 32/64 bit
#if PLATFORM_64BITS
	Props->SetStringField(TapDBConstants::PROP_OS_TYPE, TEXT("64"));
#else
	Props->SetStringField(TapDBConstants::PROP_OS_TYPE, TEXT("32"));
#endif

	// CPU architecture
	Props->SetStringField(TapDBConstants::PROP_CPU, FPlatformMisc::GetCPUBrand());

	// hardWare: CPU vendor
	Props->SetStringField(TapDBConstants::PROP_HARDWARE, FPlatformMisc::GetCPUVendor());

	// SDK info
	Props->SetStringField(TapDBConstants::PROP_SDK_VERSION, SDKVersion);

	// Session UUID (unique per app session)
	Props->SetStringField(TapDBConstants::PROP_SESSION_UUID, FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower));

	// Identity UUIDs
	Props->SetStringField(TapDBConstants::PROP_INSTALL_UUID, Identity.GetInstallationId());
	Props->SetStringField(TapDBConstants::PROP_PERSIST_UUID, Identity.GetPersistentId());
	Props->SetStringField(TapDBConstants::PROP_DEVICE_ID5, Identity.GetDeviceId5());

	// RAM: available physical memory in bytes (matches Android format)
	const FPlatformMemoryStats MemStats = FPlatformMemory::GetStats();
	Props->SetNumberField(TapDBConstants::PROP_RAM, static_cast<double>(MemStats.AvailablePhysical));

	// total_ram: total physical memory in bytes
	const FPlatformMemoryConstants& MemConstants = FPlatformMemory::GetConstants();
	Props->SetNumberField(TapDBConstants::PROP_TOTAL_RAM, static_cast<double>(MemConstants.TotalPhysical));

	// Disk space
	uint64 TotalDisk = 0;
	uint64 FreeDisk = 0;
	if (FPlatformMisc::GetDiskTotalAndFreeSpace(FPlatformProcess::BaseDir(), TotalDisk, FreeDisk))
	{
		Props->SetNumberField(TapDBConstants::PROP_ROM, static_cast<double>(FreeDisk));
		Props->SetNumberField(TapDBConstants::PROP_TOTAL_ROM, static_cast<double>(TotalDisk));
	}
	else
	{
		Props->SetNumberField(TapDBConstants::PROP_ROM, 0);
		Props->SetNumberField(TapDBConstants::PROP_TOTAL_ROM, 0);
	}

	// Screen resolution
	int32 Width = 1920;
	int32 Height = 1080;
	if (GEngine && GEngine->GameViewport)
	{
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		if (ViewportSize.X > 0 && ViewportSize.Y > 0)
		{
			Width = static_cast<int32>(ViewportSize.X);
			Height = static_cast<int32>(ViewportSize.Y);
		}
	}
	Props->SetNumberField(TapDBConstants::PROP_WIDTH, Width);
	Props->SetNumberField(TapDBConstants::PROP_HEIGHT, Height);

	// Network type: string format to match Android ("wifi" for desktop)
	Props->SetStringField(TapDBConstants::PROP_NETWORK_TYPE, TEXT("wifi"));
	Props->SetStringField(TapDBConstants::PROP_PROVIDER, TEXT("unknown"));

	// App version and channel
	Props->SetStringField(TapDBConstants::PROP_APP_VERSION, ResolveGameVersion(GameVersion));
	Props->SetStringField(TapDBConstants::PROP_CHANNEL, Channel.IsEmpty() ? TEXT("") : Channel);

	UE_LOG(LogTapDB, Verbose, TEXT("[TapDBDeviceInfo] Basic properties collected"));

	return Props;
}

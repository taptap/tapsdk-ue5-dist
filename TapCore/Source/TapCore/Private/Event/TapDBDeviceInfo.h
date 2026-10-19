// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class FTapDBIdentity;

/**
 * Collects device information as preset event properties.
 */
class FTapDBDeviceInfo
{
public:
	/**
	 * Collect basic device properties for event reporting.
	 * @param Identity The identity manager for device/install/persistent IDs
	 * @param GameVersion Game version string; empty uses the UE project version
	 * @param Channel Distribution channel
	 * @param SDKVersion SDK version string
	 * @return JSON object with all preset properties
	 */
	static TSharedPtr<FJsonObject> CollectBasicProperties(
		const FTapDBIdentity& Identity,
		const FString& GameVersion,
		const FString& Channel,
		const FString& SDKVersion);
};

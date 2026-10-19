// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapLogin.h"

class FJsonObject;

namespace TapCloudSaveDesktopToken
{
	/** Build the sdk_token object expected by the desktop CloudSave SDK. */
	TSharedRef<FJsonObject> MakeTokenObject(const TOptional<FTapAccount>& Account);

	/** Desktop CloudSave requires both fields used by MAC signing. */
	bool HasCompleteMacCredentials(const TOptional<FTapAccount>& Account);
}

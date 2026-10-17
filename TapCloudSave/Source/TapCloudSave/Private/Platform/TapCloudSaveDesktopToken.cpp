// Copyright TapTap. All Rights Reserved.

#include "Platform/TapCloudSaveDesktopToken.h"

#include "Dom/JsonObject.h"

namespace TapCloudSaveDesktopToken
{
	TSharedRef<FJsonObject> MakeTokenObject(const TOptional<FTapAccount>& Account)
	{
		TSharedRef<FJsonObject> Token = MakeShared<FJsonObject>();
		if (Account.IsSet())
		{
			Token->SetStringField(TEXT("kid"), Account->AccessToken.Kid);
			Token->SetStringField(TEXT("key"), Account->AccessToken.MacKey);
		}
		else
		{
			Token->SetStringField(TEXT("kid"), TEXT(""));
			Token->SetStringField(TEXT("key"), TEXT(""));
		}
		return Token;
	}

	bool HasCompleteMacCredentials(const TOptional<FTapAccount>& Account)
	{
		return Account.IsSet()
			&& !Account->AccessToken.Kid.IsEmpty()
			&& !Account->AccessToken.MacKey.IsEmpty();
	}
}

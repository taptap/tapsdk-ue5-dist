// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapLogin.h"

#if PLATFORM_IOS

struct FTapLoginIOSResult
{
	bool bSuccess = false;
	bool bCancelled = false;
	FString ErrorMessage;
	TOptional<FTapAccount> Account;
};

class FTapLoginIOSBridge
{
public:
	static void RegisterOpenURLHandler();
	static void UnregisterOpenURLHandler();
	static void LoginWithScopes(const TArray<FString>& Scopes, TFunction<void(FTapLoginIOSResult)> Completion);
	static bool Logout(FString& OutErrorMessage);
	static TOptional<FTapAccount> GetCurrentAccount(FString& OutErrorMessage);
};

#endif // PLATFORM_IOS

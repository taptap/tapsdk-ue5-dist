// Copyright TapTap. All Rights Reserved.

#include "TapLoginPlatformIOS.h"
#include "TapLoginModule.h"

#if PLATFORM_IOS

#include "Platform/TapLoginIOSBridge.h"

FTapLoginPlatformIOS::FTapLoginPlatformIOS()
	: bInitialized(false)
{
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] FTapLoginPlatformIOS constructor called"));
}

FTapLoginPlatformIOS::~FTapLoginPlatformIOS()
{
	FTapLoginIOSBridge::UnregisterOpenURLHandler();
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] FTapLoginPlatformIOS destructor called"));
}

void FTapLoginPlatformIOS::Initialize()
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] TapLogin Platform Initialization"));
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));

	if (bInitialized)
	{
		UE_LOG(LogTapLogin, Warning, TEXT("[iOS] TapLogin is already initialized"));
		return;
	}

	UE_LOG(LogTapLogin, Log, TEXT("[iOS] Initializing TapLogin..."));
	FTapLoginIOSBridge::RegisterOpenURLHandler();

	bInitialized = true;
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] TapLogin initialized successfully"));
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] Platform: %s"), *GetPlatformName());
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
}

bool FTapLoginPlatformIOS::IsInitialized() const
{
	UE_LOG(LogTapLogin, Verbose, TEXT("[iOS] IsInitialized called, returning: %s"), bInitialized ? TEXT("true") : TEXT("false"));
	return bInitialized;
}

void FTapLoginPlatformIOS::LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback)
{
	UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] LoginWithScopes called"));
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] Scopes count: %d"), Scopes.Num());

	if (Scopes.Num() == 0)
	{
		if (Callback.GetInterface())
		{
			Callback->OnLoginFailed(-1, TEXT("Scope cannot be empty"));
		}
		return;
	}

	// 打印所有权限范围
	for (int32 i = 0; i < Scopes.Num(); i++)
	{
		FString ScopeStr = ScopeToString(Scopes[i]);
		UE_LOG(LogTapLogin, Log, TEXT("[iOS]   Scope[%d]: %s"), i, *ScopeStr);
	}

	// 检查回调有效性
	if (!Callback.GetInterface())
	{
		UE_LOG(LogTapLogin, Error, TEXT("[iOS] Callback is null!"));
		return;
	}
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] Callback is valid"));

	const TArray<FString> ScopeStrings = ScopesToStringArray(Scopes);
	FTapLoginIOSBridge::LoginWithScopes(ScopeStrings, [Callback](FTapLoginIOSResult Result) mutable
	{
		if (!Callback.GetInterface())
		{
			UE_LOG(LogTapLogin, Warning, TEXT("[iOS] Login callback became invalid before completion"));
			return;
		}

		if (Result.bCancelled)
		{
			UE_LOG(LogTapLogin, Warning, TEXT("[iOS] Login cancelled by user"));
			Callback->OnLoginCancelled();
		}
		else if (Result.bSuccess && Result.Account.IsSet())
		{
			const FTapAccount& Account = Result.Account.GetValue();
			UE_LOG(LogTapLogin, Log, TEXT("[iOS] Login success"));
			UE_LOG(LogTapLogin, Log, TEXT("[iOS]   OpenID: %s"), *Account.OpenID);
			UE_LOG(LogTapLogin, Log, TEXT("[iOS]   UnionID: %s"), *Account.UnionID);
			Callback->OnLoginSuccess(Account);
		}
		else
		{
			const FString ErrorMessage = Result.ErrorMessage.IsEmpty() ? TEXT("TapTapLogin login failed") : Result.ErrorMessage;
			UE_LOG(LogTapLogin, Error, TEXT("[iOS] Login failed: %s"), *ErrorMessage);
			Callback->OnLoginFailed(-1, ErrorMessage);
		}

		UE_LOG(LogTapLogin, Log, TEXT("========================================"));
	});
}

void FTapLoginPlatformIOS::Logout()
{
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] Logout called"));

	FString ErrorMessage;
	if (FTapLoginIOSBridge::Logout(ErrorMessage))
	{
		UE_LOG(LogTapLogin, Log, TEXT("[iOS] Logout completed"));
	}
	else
	{
		UE_LOG(LogTapLogin, Error, TEXT("[iOS] Logout failed: %s"), *ErrorMessage);
	}
}

TOptional<FTapAccount> FTapLoginPlatformIOS::GetCurrentAccount() const
{
	UE_LOG(LogTapLogin, Log, TEXT("[iOS] GetCurrentAccount called"));

	FString ErrorMessage;
	TOptional<FTapAccount> Account = FTapLoginIOSBridge::GetCurrentAccount(ErrorMessage);
	if (Account.IsSet())
	{
		UE_LOG(LogTapLogin, Log, TEXT("[iOS] Current account found: OpenID=%s"), *Account->OpenID);
		return Account;
	}

	if (!ErrorMessage.IsEmpty())
	{
		UE_LOG(LogTapLogin, Error, TEXT("[iOS] Failed to get current account: %s"), *ErrorMessage);
	}
	else
	{
		UE_LOG(LogTapLogin, Log, TEXT("[iOS] No account found"));
	}
	return TOptional<FTapAccount>();
}

FString FTapLoginPlatformIOS::ScopeToString(ETapLoginScope Scope) const
{
	FString Result;
	switch (Scope)
	{
	case ETapLoginScope::BasicInfo:
		Result = TEXT("basic_info");
		break;
	case ETapLoginScope::PublicProfile:
		Result = TEXT("public_profile");
		break;
	case ETapLoginScope::Email:
		Result = TEXT("email");
		break;
	case ETapLoginScope::UserFriends:
		Result = TEXT("user_friends");
		break;
	case ETapLoginScope::Compliance:
		Result = TEXT("compliance");
		break;
	case ETapLoginScope::ComplianceBasic:
		Result = TEXT("compliance_basic");
		break;
	default:
		Result = TEXT("");
		UE_LOG(LogTapLogin, Warning, TEXT("[iOS] Unknown scope type: %d"), (int32)Scope);
		break;
	}

	UE_LOG(LogTapLogin, Verbose, TEXT("[iOS] Converted scope to string: %s"), *Result);
	return Result;
}

TArray<FString> FTapLoginPlatformIOS::ScopesToStringArray(const TArray<ETapLoginScope>& Scopes) const
{
	UE_LOG(LogTapLogin, Verbose, TEXT("[iOS] Converting %d scopes to string array"), Scopes.Num());

	TArray<FString> Result;
	for (int32 i = 0; i < Scopes.Num(); i++)
	{
		FString ScopeStr = ScopeToString(Scopes[i]);
		Result.Add(ScopeStr);
		UE_LOG(LogTapLogin, Verbose, TEXT("[iOS]   Converted scope[%d]: %s"), i, *ScopeStr);
	}

	UE_LOG(LogTapLogin, Verbose, TEXT("[iOS] Scope conversion completed, result count: %d"), Result.Num());
	return Result;
}

#endif // PLATFORM_IOS

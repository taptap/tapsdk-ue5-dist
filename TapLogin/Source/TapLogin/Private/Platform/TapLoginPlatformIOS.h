// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Platform/ITapLoginPlatform.h"

#if PLATFORM_IOS

/**
 * iOS 平台登录实现
 * 通过 Objective-C++ 桥接调用 iOS TapLogin SDK
 */
class FTapLoginPlatformIOS : public ITapLoginPlatform
{
public:
	FTapLoginPlatformIOS();
	virtual ~FTapLoginPlatformIOS() override;

	// ITapLoginPlatform implementation
	virtual void Initialize() override;
	virtual bool IsInitialized() const override;
	virtual void LoginWithScopes(const TArray<ETapLoginScope>& Scopes, TScriptInterface<ITapLoginCallback> Callback) override;
	virtual void Logout() override;
	virtual TOptional<FTapAccount> GetCurrentAccount() const override;
	virtual FString GetPlatformName() const override { return TEXT("iOS"); }

private:
	bool bInitialized;

	// Objective-C++ 辅助方法
	FString ScopeToString(ETapLoginScope Scope) const;
	TArray<FString> ScopesToStringArray(const TArray<ETapLoginScope>& Scopes) const;
};

#endif // PLATFORM_IOS

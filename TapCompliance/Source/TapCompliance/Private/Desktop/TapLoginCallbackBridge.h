// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TapLogin.h"
#include "TapLoginCallbackBridge.generated.h"

/**
 * 把 TFunction 桥接成 ITapLoginCallback (UObject) 的小适配器
 *
 * 仅 Desktop FastAuth 流程使用；Mobile 端不会实例化。
 *
 * 使用方式：
 *   auto* Bridge = NewObject<UTapComplianceLoginCallbackBridge>();
 *   Bridge->OnSuccess   = [&](const FTapAccount& Account) { ... };
 *   Bridge->OnFailure   = [&](FString Msg) { ... };
 *   Bridge->OnCancelled = [&]()                                                { ... };
 *   Bridge->AddToRoot();   // 防 GC
 *   FTapTapLogin::Authorize(scopes, Bridge);
 *
 * 单次回调，触发后内部自动 RemoveFromRoot 让 GC 回收。
 *
 * UCLASS 必须无条件声明 (UHT 不允许 UCLASS 在 #if 块里)。
 */
UCLASS()
class UTapComplianceLoginCallbackBridge : public UObject, public ITapLoginCallback
{
	GENERATED_BODY()

public:
	TFunction<void(const FTapAccount& /*Account*/)> OnSuccess;
	TFunction<void(int32 /*ErrorCode*/, const FString& /*ErrorMessage*/)> OnFailure;
	TFunction<void()>                                                                                       OnCancelled;

	virtual void OnLoginSuccess(const FTapAccount& Account) override
	{
		if (OnSuccess) OnSuccess(Account);
		ReleaseSelf();
	}

	virtual void OnLoginFailed(int32 ErrorCode, const FString& ErrorMessage) override
	{
		if (OnFailure) OnFailure(ErrorCode, ErrorMessage);
		ReleaseSelf();
	}

	virtual void OnLoginCancelled() override
	{
		if (OnCancelled) OnCancelled();
		ReleaseSelf();
	}

private:
	void ReleaseSelf()
	{
		OnSuccess   = nullptr;
		OnFailure   = nullptr;
		OnCancelled = nullptr;
		if (IsRooted())
		{
			RemoveFromRoot();
		}
	}
};

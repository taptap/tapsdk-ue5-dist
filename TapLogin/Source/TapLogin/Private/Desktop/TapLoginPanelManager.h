// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "UTapLoginWidget.h"

class UTapLoginWidget;

/**
 * 登录面板生命周期管理
 * 负责创建/销毁 UTapLoginWidget, 管理 Viewport 叠加
 */
class FTapLoginPanelManager
{
public:
	static FTapLoginPanelManager& Get();

	/**
	 * 显示登录面板
	 * @param ClientID TapTap 应用 ClientID
	 * @param ScopeStrings 权限字符串数组
	 * @param OnClose 面板关闭回调
	 * @param OnWebLogin 网页登录按钮回调
	 * @param OnAuthSuccess QR 码认证成功回调
	 */
	void ShowPanel(
		const FString& ClientID,
		const TArray<FString>& ScopeStrings,
		FOnLoginPanelClose OnClose,
		FOnLoginPanelWebLogin OnWebLogin,
		FOnLoginPanelAuthSuccess OnAuthSuccess);

	/** 关闭面板 */
	void ClosePanel();

	/** 通知面板 Web 登录失败 */
	void OnWebLoginFailed();

	/** 显示 Web 登录取消 Toast */
	void ShowWebLoginCancelledToast();

	/** 面板是否正在显示 */
	bool IsPanelShowing() const;

private:
	FTapLoginPanelManager() = default;

	// Blueprint 类引用 (首次使用时加载)
	TSubclassOf<UTapLoginWidget> LoginWidgetClass;

	// 使用 TWeakObjectPtr: 非 UObject 类无法用 UPROPERTY,
	// widget 通过 AddToViewport 保持 GC 引用
	TWeakObjectPtr<UTapLoginWidget> LoginWidget;
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#include "TapLoginPanelManager.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapLoginModule.h"
#include "UTapLoginWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"

FTapLoginPanelManager& FTapLoginPanelManager::Get()
{
	static FTapLoginPanelManager Instance;
	return Instance;
}

void FTapLoginPanelManager::ShowPanel(
	const FString& ClientID,
	const TArray<FString>& ScopeStrings,
	FOnLoginPanelClose OnClose,
	FOnLoginPanelWebLogin OnWebLogin,
	FOnLoginPanelAuthSuccess OnAuthSuccess)
{
	// 如果已经有面板, 先关闭
	if (LoginWidget.IsValid())
	{
		ClosePanel();
	}

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[PanelManager] No GameViewport available"));
		OnClose.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[PanelManager] No World available"));
		OnClose.ExecuteIfBound();
		return;
	}

	// 首次使用时加载 Blueprint 类
	if (!LoginWidgetClass)
	{
		LoginWidgetClass = LoadClass<UTapLoginWidget>(nullptr,
			TEXT("/TapLogin/UI/WBP_TapLoginPanel.WBP_TapLoginPanel_C"));
		if (!LoginWidgetClass)
		{
			UE_LOG(LogTapLogin, Error, TEXT("[PanelManager] Failed to load WBP_TapLoginPanel"));
			OnClose.ExecuteIfBound();
			return;
		}
	}

	// 使用 Blueprint 类创建 UMG widget
	UTapLoginWidget* Widget = CreateWidget<UTapLoginWidget>(World, LoginWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[PanelManager] Failed to create UTapLoginWidget"));
		OnClose.ExecuteIfBound();
		return;
	}

	Widget->Init(ClientID, ScopeStrings, OnClose, OnWebLogin, OnAuthSuccess);
	Widget->AddToViewport(1000);
	LoginWidget = Widget;

	UE_LOG(LogTapLogin, Log, TEXT("[PanelManager] Login panel shown (Blueprint)"));
}

void FTapLoginPanelManager::OnWebLoginFailed()
{
	if (LoginWidget.IsValid())
	{
		LoginWidget->NotifyWebLoginFailed();
	}
}

void FTapLoginPanelManager::ShowWebLoginCancelledToast()
{
	if (LoginWidget.IsValid())
	{
		LoginWidget->ShowWebLoginCancelledToast();
	}
}

void FTapLoginPanelManager::ClosePanel()
{
	if (LoginWidget.IsValid())
	{
		LoginWidget->RemoveFromParent();
	}
	LoginWidget.Reset();

	UE_LOG(LogTapLogin, Log, TEXT("[PanelManager] Login panel closed"));
}

bool FTapLoginPanelManager::IsPanelShowing() const
{
	return LoginWidget.IsValid() && LoginWidget->IsInViewport();
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

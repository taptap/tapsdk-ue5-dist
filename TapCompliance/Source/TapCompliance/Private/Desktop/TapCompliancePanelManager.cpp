// Copyright TapTap. All Rights Reserved.

#include "TapCompliancePanelManager.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapComplianceModule.h"
#include "UTapComplianceRealNameWidget.h"
#include "UTapComplianceFastAuthWidget.h"
#include "UTapComplianceAuthConfirmWidget.h"
#include "UTapComplianceAuthFailedWidget.h"
#include "UTapComplianceAuthReviewingWidget.h"
#include "UTapComplianceHealthAuthRequiredWidget.h"
#include "UTapComplianceHealthMinorTimeLimitWidget.h"
#include "UTapComplianceHealthMinorPaymentLimitWidget.h"
#include "UTapComplianceLoadingWidget.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Misc/PackageName.h"

FTapCompliancePanelManager& FTapCompliancePanelManager::Get()
{
	static FTapCompliancePanelManager Instance;
	return Instance;
}

void FTapCompliancePanelManager::ShowPanel(
	FOnCompliancePanelClose OnClose,
	FOnCompliancePanelBack OnBack,
	FOnCompliancePanelSubmit OnSubmit,
	bool bHasAuthRecord)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available"));
		OnClose.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available"));
		OnClose.ExecuteIfBound();
		return;
	}

	// 首次使用时加载 Blueprint 类
	if (!RealNameWidgetClass)
	{
		RealNameWidgetClass = LoadClass<UTapComplianceRealNameWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceRealNamePanel.TapComplianceRealNamePanel_C"));
		if (!RealNameWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceRealNamePanel"));
			OnClose.ExecuteIfBound();
			return;
		}
	}

	// 使用 Blueprint 类创建 UMG widget
	UTapComplianceRealNameWidget* Widget = CreateWidget<UTapComplianceRealNameWidget>(World, RealNameWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceRealNameWidget"));
		OnClose.ExecuteIfBound();
		return;
	}

	Widget->Init(OnClose, OnBack, OnSubmit, bHasAuthRecord);
	Widget->AddToViewport(1000);
	RealNameWidget = Widget;
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log,
		TEXT("[PanelManager] RealName panel shown (Blueprint) bHasAuthRecord=%s"),
		bHasAuthRecord ? TEXT("true") : TEXT("false"));
}

void FTapCompliancePanelManager::OnSubmitFailed(const FString& ErrorMessage)
{
	if (RealNameWidget.IsValid())
	{
		RealNameWidget->NotifySubmitFailed(ErrorMessage);
	}
}

void FTapCompliancePanelManager::OnSubmitSuccess()
{
	if (RealNameWidget.IsValid())
	{
		RealNameWidget->NotifySubmitSuccess();
	}
}

void FTapCompliancePanelManager::ShowFastAuthPanel(
	bool                 bShowCancelButton,
	FOnFastAuthConfirmed OnConfirmed,
	FOnFastAuthCancelled OnCancelled,
	FOnFastAuthClosed    OnClosed)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available (FastAuth)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available (FastAuth)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	if (!FastAuthWidgetClass)
	{
		FastAuthWidgetClass = LoadClass<UTapComplianceFastAuthWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceFastAuthPanel.TapComplianceFastAuthPanel_C"));
		if (!FastAuthWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceFastAuthPanel"));
			OnClosed.ExecuteIfBound();
			return;
		}
	}

	UTapComplianceFastAuthWidget* Widget = CreateWidget<UTapComplianceFastAuthWidget>(World, FastAuthWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceFastAuthWidget"));
		OnClosed.ExecuteIfBound();
		return;
	}

	Widget->Init(OnConfirmed, OnCancelled, OnClosed, bShowCancelButton);
	Widget->AddToViewport(1000);
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log,
		TEXT("[PanelManager] FastAuth panel shown (showCancel=%s)"),
		bShowCancelButton ? TEXT("true") : TEXT("false"));
}

void FTapCompliancePanelManager::ShowAuthReviewingPanel(
	EAuthReviewingState State,
	FOnAuthReviewingConfirmed OnConfirmed,
	FOnAuthReviewingClosed OnClosed)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available (AuthReviewing)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available (AuthReviewing)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	if (!AuthReviewingWidgetClass)
	{
		AuthReviewingWidgetClass = LoadClass<UTapComplianceAuthReviewingWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceAuthReviewingPanel.TapComplianceAuthReviewingPanel_C"));
		if (!AuthReviewingWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceAuthReviewingPanel"));
			OnClosed.ExecuteIfBound();
			return;
		}
	}

	UTapComplianceAuthReviewingWidget* Widget =
		CreateWidget<UTapComplianceAuthReviewingWidget>(World, AuthReviewingWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceAuthReviewingWidget"));
		OnClosed.ExecuteIfBound();
		return;
	}

	Widget->Init(State, OnConfirmed, OnClosed);
	Widget->AddToViewport(1000);
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log,
		TEXT("[PanelManager] AuthReviewing panel shown (State=%s)"),
		State == EAuthReviewingState::Waiting ? TEXT("Waiting") : TEXT("Failed"));
}

void FTapCompliancePanelManager::ShowAuthConfirmPanel(
	const FString& MaskedName,
	const FString& MaskedIDNumber,
	FOnAuthConfirmConfirmed OnConfirmed,
	FOnAuthConfirmClosed OnClosed)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available (AuthConfirm)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available (AuthConfirm)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	if (!AuthConfirmWidgetClass)
	{
		AuthConfirmWidgetClass = LoadClass<UTapComplianceAuthConfirmWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceAuthConfirmPanel.TapComplianceAuthConfirmPanel_C"));
		if (!AuthConfirmWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceAuthConfirmPanel"));
			OnClosed.ExecuteIfBound();
			return;
		}
	}

	UTapComplianceAuthConfirmWidget* Widget = CreateWidget<UTapComplianceAuthConfirmWidget>(World, AuthConfirmWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceAuthConfirmWidget"));
		OnClosed.ExecuteIfBound();
		return;
	}

	Widget->Init(OnConfirmed, OnClosed);
	Widget->SetUserInfo(MaskedName, MaskedIDNumber);
	Widget->AddToViewport(1000);
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log, TEXT("[PanelManager] AuthConfirm panel shown (Name=%s, ID=%s)"),
		MaskedName.IsEmpty() ? TEXT("(empty)") : *MaskedName,
		MaskedIDNumber.IsEmpty() ? TEXT("(empty)") : *MaskedIDNumber);
}

void FTapCompliancePanelManager::ShowAuthFailedPanel(
	FOnAuthFailedConfirmed OnConfirmed,
	FOnAuthFailedClosed OnClosed)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available (AuthFailed)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available (AuthFailed)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	if (!AuthFailedWidgetClass)
	{
		AuthFailedWidgetClass = LoadClass<UTapComplianceAuthFailedWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceAuthFailedPanel.TapComplianceAuthFailedPanel_C"));
		if (!AuthFailedWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceAuthFailedPanel"));
			OnClosed.ExecuteIfBound();
			return;
		}
	}

	UTapComplianceAuthFailedWidget* Widget = CreateWidget<UTapComplianceAuthFailedWidget>(World, AuthFailedWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceAuthFailedWidget"));
		OnClosed.ExecuteIfBound();
		return;
	}

	Widget->Init(OnConfirmed, OnClosed);
	Widget->AddToViewport(1000);
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log, TEXT("[PanelManager] AuthFailed panel shown"));
}

void FTapCompliancePanelManager::ShowHealthAuthRequiredPanel(
	bool                               bShowSwitchAccount,
	FOnHealthAuthRequiredSwitchAccount OnSwitchAccount,
	FOnHealthAuthRequiredExitGame      OnExitGame,
	FOnHealthAuthRequiredGoAuth        OnGoAuth,
	FOnHealthAuthRequiredClosed        OnClosed)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available (HealthAuthRequired)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available (HealthAuthRequired)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	if (!HealthAuthRequiredWidgetClass)
	{
		HealthAuthRequiredWidgetClass = LoadClass<UTapComplianceHealthAuthRequiredWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceHealthAuthRequiredPanel.TapComplianceHealthAuthRequiredPanel_C"));
		if (!HealthAuthRequiredWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceHealthAuthRequiredPanel"));
			OnClosed.ExecuteIfBound();
			return;
		}
	}

	UTapComplianceHealthAuthRequiredWidget* Widget = CreateWidget<UTapComplianceHealthAuthRequiredWidget>(World, HealthAuthRequiredWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceHealthAuthRequiredWidget"));
		OnClosed.ExecuteIfBound();
		return;
	}

	Widget->Init(OnSwitchAccount, OnExitGame, OnGoAuth, OnClosed, bShowSwitchAccount);
	Widget->AddToViewport(1000);
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log,
		TEXT("[PanelManager] HealthAuthRequired panel shown (showSwitchAccount=%s)"),
		bShowSwitchAccount ? TEXT("true") : TEXT("false"));
}

void FTapCompliancePanelManager::ShowHealthMinorTimeLimitPanel(
	bool                                 bAllowEnterGame,
	const FText&                         Title,
	const FText&                         Content,
	FOnHealthMinorTimeLimitExitGame      OnExitGame,
	FOnHealthMinorTimeLimitSwitchAccount OnSwitchAccount,
	FOnHealthMinorTimeLimitClosed        OnClosed)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available (HealthMinorTimeLimit)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available (HealthMinorTimeLimit)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	if (!HealthMinorTimeLimitWidgetClass)
	{
		HealthMinorTimeLimitWidgetClass = LoadClass<UTapComplianceHealthMinorTimeLimitWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceHealthMinorTimeLimitPanel.TapComplianceHealthMinorTimeLimitPanel_C"));
		if (!HealthMinorTimeLimitWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceHealthMinorTimeLimitPanel"));
			OnClosed.ExecuteIfBound();
			return;
		}
	}

	UTapComplianceHealthMinorTimeLimitWidget* Widget = CreateWidget<UTapComplianceHealthMinorTimeLimitWidget>(World, HealthMinorTimeLimitWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceHealthMinorTimeLimitWidget"));
		OnClosed.ExecuteIfBound();
		return;
	}

	Widget->Init(OnExitGame, OnSwitchAccount, OnClosed);
	Widget->AddToViewport(1000);
	// 动态文案: Title / Content 空时 Widget setter 不调 SetText, 保留 WBP 默认作最后兜底
	Widget->SetTitle(Title);
	Widget->SetContent(Content);
	Widget->SetExitButtonText(bAllowEnterGame
		? NSLOCTEXT("TapCompliance", "Btn_EnterGame", "进入游戏")
		: NSLOCTEXT("TapCompliance", "Btn_ExitGame",  "退出游戏"));
	// 仅当调用方明确绑定了 OnSwitchAccount 才显示按钮 (与 Unity 行为对齐:
	//   switchAccountButton.gameObject.SetActive(onSwitchAccount != null))
	Widget->SetSwitchAccountVisible(OnSwitchAccount.IsBound());
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log,
		TEXT("[PanelManager] HealthMinorTimeLimit panel shown (allowEnter=%s, switchAccount=%s, hasTitle=%s, hasContent=%s)"),
		bAllowEnterGame             ? TEXT("true")  : TEXT("false"),
		OnSwitchAccount.IsBound()   ? TEXT("on")    : TEXT("off"),
		Title.IsEmpty()             ? TEXT("false") : TEXT("true"),
		Content.IsEmpty()           ? TEXT("false") : TEXT("true"));
}

void FTapCompliancePanelManager::ShowHealthMinorPaymentLimitPanel(
	const FText&                         Title,
	const FText&                         Content,
	FOnHealthMinorPaymentLimitReturnGame OnReturnGame,
	FOnHealthMinorPaymentLimitClosed     OnClosed)
{
	// 同时只允许一个面板, 先关闭已有面板
	CloseAllPanels();

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No GameViewport available (HealthMinorPaymentLimit)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] No World available (HealthMinorPaymentLimit)"));
		OnClosed.ExecuteIfBound();
		return;
	}

	if (!HealthMinorPaymentLimitWidgetClass)
	{
		HealthMinorPaymentLimitWidgetClass = LoadClass<UTapComplianceHealthMinorPaymentLimitWidget>(nullptr,
			TEXT("/TapCompliance/UI/TapComplianceHealthMinorPaymentLimitPanel.TapComplianceHealthMinorPaymentLimitPanel_C"));
		if (!HealthMinorPaymentLimitWidgetClass)
		{
			UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to load TapComplianceHealthMinorPaymentLimitPanel"));
			OnClosed.ExecuteIfBound();
			return;
		}
	}

	UTapComplianceHealthMinorPaymentLimitWidget* Widget = CreateWidget<UTapComplianceHealthMinorPaymentLimitWidget>(World, HealthMinorPaymentLimitWidgetClass);
	if (!Widget)
	{
		UE_LOG(LogTapCompliance, Error, TEXT("[PanelManager] Failed to create UTapComplianceHealthMinorPaymentLimitWidget"));
		OnClosed.ExecuteIfBound();
		return;
	}

	Widget->Init(OnReturnGame, OnClosed);
	Widget->AddToViewport(1000);
	// 动态文案: Title / Content 空时 Widget setter 不调 SetText, 保留 WBP 默认作最后兜底
	Widget->SetTitle(Title);
	Widget->SetContent(Content);
	RegisterPanel(Widget);

	UE_LOG(LogTapCompliance, Log,
		TEXT("[PanelManager] HealthMinorPaymentLimit panel shown (hasTitle=%s, hasContent=%s)"),
		Title.IsEmpty()   ? TEXT("false") : TEXT("true"),
		Content.IsEmpty() ? TEXT("false") : TEXT("true"));
}

void FTapCompliancePanelManager::RegisterPanel(UUserWidget* Widget)
{
	if (Widget)
	{
		ActivePanels.Add(Widget);
	}
}

void FTapCompliancePanelManager::CloseAllPanels()
{
	// Loading 蒙层与业务面板同源关闭: 任何 ShowXxxPanel 走到这里时都会顺带把
	// Loading 收掉, 对齐 Unity UIManager 在 panel 出现后自动 CloseLoading 的语义.
	UTapComplianceLoadingWidget::HideLoading();

	int32 ClosedCount = 0;
	for (const TWeakObjectPtr<UUserWidget>& WeakWidget : ActivePanels)
	{
		if (WeakWidget.IsValid())
		{
			WeakWidget->RemoveFromParent();
			++ClosedCount;
		}
	}
	ActivePanels.Reset();
	RealNameWidget.Reset();

	if (ClosedCount > 0)
	{
		UE_LOG(LogTapCompliance, Log, TEXT("[PanelManager] Closed %d panel(s)"), ClosedCount);
	}
}

bool FTapCompliancePanelManager::IsAnyPanelShowing() const
{
	for (const TWeakObjectPtr<UUserWidget>& WeakWidget : ActivePanels)
	{
		if (WeakWidget.IsValid() && WeakWidget->IsInViewport())
		{
			return true;
		}
	}
	return false;
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

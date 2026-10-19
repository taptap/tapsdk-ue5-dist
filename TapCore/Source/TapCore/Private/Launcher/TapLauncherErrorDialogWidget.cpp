// Copyright TapTap. All Rights Reserved.

#include "Launcher/TapLauncherErrorDialogWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"

DEFINE_LOG_CATEGORY_STATIC(LogTapLauncherErrorDialog, Log, All);

namespace
{
	const TCHAR* DefaultDialogClassPath =
		TEXT("/TapCore/WBP_TapCoreLanuchErrorDialog.WBP_TapCoreLanuchErrorDialog_C");
	const TCHAR* TapTapDownloadUrl =
		TEXT("https://www.taptap.cn/mobile?utm_medium=coop&utm_source=pc_toStart");

	TSubclassOf<UTapLauncherErrorDialogWidget> RegisteredDialogClass;
	TWeakObjectPtr<UTapLauncherErrorDialogWidget> ActiveDialog;
}

void UTapLauncherErrorDialogWidget::RegisterErrorDialogClass(
	TSubclassOf<UTapLauncherErrorDialogWidget> InDialogClass)
{
	RegisteredDialogClass = InDialogClass;

	UE_LOG(LogTapLauncherErrorDialog, Log,
		TEXT("Launcher error dialog class %s"),
		RegisteredDialogClass
			? *RegisteredDialogClass->GetPathName()
			: TEXT("cleared"));
}

bool UTapLauncherErrorDialogWidget::IsErrorDialogClassRegistered()
{
	return RegisteredDialogClass != nullptr;
}

bool UTapLauncherErrorDialogWidget::ShowErrorDialog(
	ETapLauncherInitResult InResult,
	const FString& InNativeErrorMessage)
{
	UE_LOG(LogTapLauncherErrorDialog, Warning,
		TEXT("ShowErrorDialog begin: result=%d, registered=%d"),
		static_cast<int32>(InResult), RegisteredDialogClass ? 1 : 0);

	if (!RegisteredDialogClass)
	{
		RegisteredDialogClass = LoadClass<UTapLauncherErrorDialogWidget>(
			nullptr, DefaultDialogClassPath);
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("Default dialog class load: path=%s, loaded=%d"),
			DefaultDialogClassPath, RegisteredDialogClass ? 1 : 0);
	}

	if (!RegisteredDialogClass)
	{
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("Cannot show launcher error dialog: no registered class and default Blueprint is missing at %s"),
			DefaultDialogClassPath);
		return false;
	}

	UGameViewportClient* Viewport = GEngine ? GEngine->GameViewport : nullptr;
	if (!Viewport)
	{
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("Cannot show launcher error dialog: no GameViewport available"));
		return false;
	}

	UWorld* World = Viewport->GetWorld();
	if (!World)
	{
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("Cannot show launcher error dialog: no World available"));
		return false;
	}

	if (ActiveDialog.IsValid() && ActiveDialog->IsInViewport())
	{
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("Updating active launcher error dialog: %s"),
			*ActiveDialog->GetPathName());
		ActiveDialog->SetError(InResult, InNativeErrorMessage);
		return true;
	}

	ActiveDialog.Reset();

	UTapLauncherErrorDialogWidget* Dialog =
		CreateWidget<UTapLauncherErrorDialogWidget>(World, RegisteredDialogClass);
	if (!Dialog)
	{
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("Cannot show launcher error dialog: widget creation failed"));
		return false;
	}

	Dialog->AddToViewport(1000);
	ActiveDialog = Dialog;
	UE_LOG(LogTapLauncherErrorDialog, Warning,
		TEXT("Launcher error dialog added: %s, inViewport=%d"),
		*Dialog->GetPathName(), Dialog->IsInViewport() ? 1 : 0);
	Dialog->SetError(InResult, InNativeErrorMessage);
	return true;
}

void UTapLauncherErrorDialogWidget::SetError(
	ETapLauncherInitResult InResult,
	const FString& InNativeErrorMessage)
{
	ErrorResult = InResult;
	NativeErrorMessage = InNativeErrorMessage;
	ErrorMessage = GetErrorMessage(InResult);
	if (ErrorText)
	{
		ErrorText->SetText(ErrorMessage);
	}
	OnErrorUpdated();
}

void UTapLauncherErrorDialogWidget::OpenTapTapDownloadPage()
{
	FPlatformProcess::LaunchURL(TapTapDownloadUrl, nullptr, nullptr);
}

void UTapLauncherErrorDialogWidget::ConfirmAndQuitGame()
{
	UE_LOG(LogTapLauncherErrorDialog, Warning,
		TEXT("ConfirmAndQuitGame called: %s"), *GetPathName());
	CloseDialog();
	FPlatformMisc::RequestExit(false);
}

void UTapLauncherErrorDialogWidget::CloseDialog()
{
	RemoveFromParent();
}

FText UTapLauncherErrorDialogWidget::GetErrorMessage(
	ETapLauncherInitResult InResult)
{
	switch (InResult)
	{
	case ETapLauncherInitResult::NoPlatform:
		return FText::FromString(
			TEXT("获取游戏信息失败，请下载 TapTap 客户端后重新启动游戏"));
	case ETapLauncherInitResult::NotLaunchedByPlatform:
		return FText::FromString(
			TEXT("获取游戏信息失败，请从 TapTap 客户端重新启动游戏"));
	case ETapLauncherInitResult::PlatformVersionMismatch:
		return FText::FromString(
			TEXT("客户端版本不匹配，请将 TapTap 客户端升级到最新版本"));
	case ETapLauncherInitResult::Unknown:
		return FText::FromString(
			TEXT("本地发生未知错误，请从 TapTap 客户端重新启动游戏"));
	default:
		return FText::FromString(
			TEXT("发生未知错误，请从 TapTap 客户端重新启动游戏"));
	}
}

void UTapLauncherErrorDialogWidget::NativeConstruct()
{
	Super::NativeConstruct();

	UE_LOG(LogTapLauncherErrorDialog, Warning,
		TEXT("NativeConstruct: class=%s, InstallTipButton=%d, Button_Confirm=%d, ErrorText=%d"),
		*GetClass()->GetPathName(), InstallTipButton ? 1 : 0, Button_Confirm ? 1 : 0,
		ErrorText ? 1 : 0);

	if (InstallTipButton)
	{
		InstallTipButton->OnClicked.AddUniqueDynamic(
			this,
			&UTapLauncherErrorDialogWidget::OnInstallTipClicked);
	}
	else
	{
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("InstallTipButton was not bound; expected a Button named InstallTipButton"));
	}

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.AddUniqueDynamic(
			this,
			&UTapLauncherErrorDialogWidget::OnOKClicked);
	}
	else
	{
		UE_LOG(LogTapLauncherErrorDialog, Error,
			TEXT("Button_Confirm was not bound; expected a Button named Button_Confirm"));
	}

	if (!ErrorText)
	{
		UE_LOG(LogTapLauncherErrorDialog, Warning,
			TEXT("ErrorText was not bound; expected a TextBlock named ErrorText"));
	}
}

void UTapLauncherErrorDialogWidget::OnInstallTipClicked()
{
	UE_LOG(LogTapLauncherErrorDialog, Warning, TEXT("InstallTipButton clicked"));
	OpenTapTapDownloadPage();
}

void UTapLauncherErrorDialogWidget::OnOKClicked()
{
	UE_LOG(LogTapLauncherErrorDialog, Warning, TEXT("Button_Confirm clicked"));
	ConfirmAndQuitGame();
}

void UTapLauncherErrorDialogWidget::NativeDestruct()
{
	UE_LOG(LogTapLauncherErrorDialog, Warning,
		TEXT("NativeDestruct: class=%s"), *GetClass()->GetPathName());

	if (InstallTipButton)
	{
		InstallTipButton->OnClicked.RemoveDynamic(
			this,
			&UTapLauncherErrorDialogWidget::OnInstallTipClicked);
	}

	if (Button_Confirm)
	{
		Button_Confirm->OnClicked.RemoveDynamic(
			this,
			&UTapLauncherErrorDialogWidget::OnOKClicked);
	}

	if (ActiveDialog.Get() == this)
	{
		ActiveDialog.Reset();
	}

	Super::NativeDestruct();
}

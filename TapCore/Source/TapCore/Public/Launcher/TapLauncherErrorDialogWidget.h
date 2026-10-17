// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Launcher/TapLauncherTypes.h"
#include "TapLauncherErrorDialogWidget.generated.h"

class UButton;
class UTextBlock;

/**
 * Blueprint base class for the PC launcher startup error dialog.
 *
 * The game owns the Blueprint subclass and its visual layout. TapCore only
 * provides the launcher error data and the two Unity-compatible actions:
 * opening the TapTap download page and exiting the game.
 */
UCLASS(Abstract, Blueprintable)
class TAPCORE_API UTapLauncherErrorDialogWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Register a game-specific Blueprint subclass, overriding the TapCore default. */
	UFUNCTION(BlueprintCallable, Category = "TapLauncher|ErrorDialog")
	static void RegisterErrorDialogClass(
		TSubclassOf<UTapLauncherErrorDialogWidget> InDialogClass);

	/** Returns whether an override class has been registered. */
	UFUNCTION(BlueprintPure, Category = "TapLauncher|ErrorDialog")
	static bool IsErrorDialogClassRegistered();

	/**
	 * Show or update the registered error dialog.
	 * @return false when no class or playable game world is available.
	 */
	static bool ShowErrorDialog(
		ETapLauncherInitResult InResult,
		const FString& InNativeErrorMessage);

	/** Set the error data and notify the Blueprint presentation layer. */
	UFUNCTION(BlueprintCallable, Category = "TapLauncher|ErrorDialog")
	void SetError(
		ETapLauncherInitResult InResult,
		const FString& InNativeErrorMessage);

	/** Error code returned by TapTap PC launcher verification. */
	UPROPERTY(BlueprintReadOnly, Category = "TapLauncher|ErrorDialog")
	ETapLauncherInitResult ErrorResult = ETapLauncherInitResult::Unknown;

	/** Unity-compatible text intended for display in the dialog. */
	UPROPERTY(BlueprintReadOnly, Category = "TapLauncher|ErrorDialog")
	FText ErrorMessage;

	/** Raw native error text, available for diagnostics or custom Blueprint copy. */
	UPROPERTY(BlueprintReadOnly, Category = "TapLauncher|ErrorDialog")
	FString NativeErrorMessage;

	/** Called after C++ updates ErrorResult, ErrorMessage, and NativeErrorMessage. */
	UFUNCTION(BlueprintImplementableEvent, Category = "TapLauncher|ErrorDialog")
	void OnErrorUpdated();

	/** Open the TapTap PC client download page and keep the dialog open. */
	UFUNCTION(BlueprintCallable, Category = "TapLauncher|ErrorDialog")
	void OpenTapTapDownloadPage();

	/** Close the dialog and request game exit, matching Unity's OK button. */
	UFUNCTION(BlueprintCallable, Category = "TapLauncher|ErrorDialog")
	void ConfirmAndQuitGame();

	/** Remove this dialog from the viewport without exiting the game. */
	UFUNCTION(BlueprintCallable, Category = "TapLauncher|ErrorDialog")
	void CloseDialog();

	/** Return the default Unity-compatible message for an error code. */
	UFUNCTION(BlueprintPure, Category = "TapLauncher|ErrorDialog")
	static FText GetErrorMessage(ETapLauncherInitResult InResult);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	/** Bound to the Blueprint install button when present. */
	UFUNCTION()
	void OnInstallTipClicked();

	/** Bound to the Blueprint OK button when present. */
	UFUNCTION()
	void OnOKClicked();

	/** Install TapTap button. Blueprint widget name: InstallTipButton. */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* InstallTipButton = nullptr;

	/** Confirm/quit button. Blueprint widget name: Button_Confirm. */
	UPROPERTY(meta = (BindWidgetOptional))
	UButton* Button_Confirm = nullptr;

	/** Error/body text block. Blueprint widget name: ErrorText. */
	UPROPERTY(meta = (BindWidgetOptional))
	UTextBlock* ErrorText = nullptr;
};

// Copyright TapTap. All Rights Reserved.

#include "UTapLoginWidget.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "TapLoginModule.h"
#include "TapQRCodeGenerator.h"
#include "TapQRCodeService.h"
#include "TapTapSdkOptions.h"
#include "Internal/TapSdkConfigState.h"

// UMG
#include "Components/Image.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Blueprint/WidgetTree.h"
#include "TimerManager.h"
#include "Containers/Ticker.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"

static const float QRCodeSize = 160.0f;

UTapLoginWidget::UTapLoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	static ConstructorHelpers::FObjectFinder<UTexture2D> ToastSuccessFinder(TEXT("/TapLogin/Textures/login_toast_success"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> ToastLoadingFinder(TEXT("/TapLogin/Textures/login_toast_loading"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> ToastCancelFinder(TEXT("/TapLogin/Textures/login_toast_cancel"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> ToastFailFinder(TEXT("/TapLogin/Textures/login_toast_fail"));

	ToastSuccess = ToastSuccessFinder.Object;
	ToastLoading = ToastLoadingFinder.Object;
	ToastCancel = ToastCancelFinder.Object;
	ToastFail = ToastFailFinder.Object;


	// 登录指引纹理 (cn / global)
	static ConstructorHelpers::FObjectFinder<UTexture2D> IOSGuideCN(TEXT("/TapLogin/Textures/login_guide_ios_cn"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> IOSGuideGlobal(TEXT("/TapLogin/Textures/login_guide_ios_global"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> AndroidGuideCN(TEXT("/TapLogin/Textures/login_guide_android_cn"));
	static ConstructorHelpers::FObjectFinder<UTexture2D> AndroidGuideGlobal(TEXT("/TapLogin/Textures/login_guide_android_global"));

	bool bIsCN = (FTapSdkConfigState::GetCurrentConfig().Region == ETapRegion::CN);
	GuideIOSTexture = bIsCN ? IOSGuideCN.Object : IOSGuideGlobal.Object;
	GuideAndroidTexture = bIsCN ? AndroidGuideCN.Object : AndroidGuideGlobal.Object;
}

// =============================================================================
// Init / NativeConstruct / NativeDestruct
// =============================================================================

void UTapLoginWidget::Init(
	const FString& InClientID,
	const TArray<FString>& InScopeStrings,
	FOnLoginPanelClose InOnClose,
	FOnLoginPanelWebLogin InOnWebLogin,
	FOnLoginPanelAuthSuccess InOnAuthSuccess)
{
	ClientID = InClientID;
	ScopeStrings = InScopeStrings;
	OnClose = InOnClose;
	OnWebLogin = InOnWebLogin;
	OnAuthSuccess = InOnAuthSuccess;
}

void UTapLoginWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// BindWidget 已自动绑定好控件引用, 绑定按钮事件
	if (CloseBtn) CloseBtn->OnClicked.AddDynamic(this, &UTapLoginWidget::OnCloseClicked);
	if (RefreshBtn) RefreshBtn->OnClicked.AddDynamic(this, &UTapLoginWidget::OnRefreshClicked);
	if (WebBtn) WebBtn->OnClicked.AddDynamic(this, &UTapLoginWidget::OnWebLoginClicked);

	// 绑定 iOS/Android 按钮 hover 事件
	if (IOSBtn)
	{
		IOSBtn->OnHovered.AddDynamic(this, &UTapLoginWidget::OnIOSBtnHovered);
		IOSBtn->OnUnhovered.AddDynamic(this, &UTapLoginWidget::OnIOSBtnUnhovered);
	}
	if (AndroidBtn)
	{
		AndroidBtn->OnHovered.AddDynamic(this, &UTapLoginWidget::OnAndroidBtnHovered);
		AndroidBtn->OnUnhovered.AddDynamic(this, &UTapLoginWidget::OnAndroidBtnUnhovered);
	}

	// 去掉 QR 码区域的边框线 (保留背景色, 描边颜色设为与背景色一致消除锯齿边缘)
	auto ClearBorderOutline = [](UBorder* Border)
	{
		if (!Border) return;
		FSlateBrush Brush = Border->Background;
		// 用与填充色相同的描边覆盖 RoundedBox 抗锯齿边缘
		FLinearColor FillColor = Brush.TintColor.GetSpecifiedColor();
		Brush.OutlineSettings.Width = 2.0f;
		Brush.OutlineSettings.Color = FSlateColor(FillColor);
		Brush.OutlineSettings.RoundingType = Brush.OutlineSettings.RoundingType;
		// CornerRadii 保持 Blueprint 中的值不变
		Border->SetBrush(Brush);
	};
	ClearBorderOutline(QROuterBorder);
	ClearBorderOutline(QRInnerBorder);

	// 默认隐藏指引浮窗
	if (GuideArea)
	{
		GuideArea->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 初始化 UI 状态: 刷新按钮隐藏, toast 隐藏
	if (RefreshBtn) RefreshBtn->SetVisibility(ESlateVisibility::Collapsed);
	if (ToastImage)
	{
		ToastImage->SetColorAndOpacity(FLinearColor::White);
		ToastImage->SetRenderOpacity(1.0f);
		ToastImage->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
	}
	HideToast();

	// 启动 QR 服务
	QRService = MakeShared<FTapQRCodeService>();
	RequestQRCode();
}

void UTapLoginWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ToastTimerHandle);
	}
	if (ToastTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ToastTickerHandle);
		ToastTickerHandle.Reset();
	}

	if (QRService)
	{
		QRService->StopPolling();
		QRService.Reset();
	}
	if (QRCodeImage)
	{
		QRCodeImage->SetBrush(FSlateBrush());
	}
	QRCodeTexture = nullptr;

	Super::NativeDestruct();
}

// =============================================================================
// 顶部 Toast 通知
// =============================================================================

void UTapLoginWidget::ShowToast(UTexture2D* ToastTexture)
{
	if (ToastImage && ToastTexture)
	{
		ToastImage->SetBrushFromTexture(ToastTexture);
		const float FixedHeight = 100.0f;
		const float Aspect = (float)ToastTexture->GetSizeX() / (float)ToastTexture->GetSizeY();
		const FVector2D DesiredSize(FixedHeight * Aspect, FixedHeight);
		ToastImage->SetDesiredSizeOverride(DesiredSize);
		ToastImage->SetVisibility(ESlateVisibility::Visible);
	}

	// 5 秒后自动隐藏 (优先使用 World Timer，fallback 到 Ticker)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ToastTimerHandle);
		World->GetTimerManager().SetTimer(ToastTimerHandle,
			FTimerDelegate::CreateUObject(this, &UTapLoginWidget::HideToast),
			5.0f, false);
	}
	else
	{
		// 无 World 时使用 FTSTicker
		ToastTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis = TWeakObjectPtr<UTapLoginWidget>(this)](float) -> bool
			{
				if (UTapLoginWidget* Self = WeakThis.Get())
				{
					Self->HideToast();
				}
				return false; // 单次触发
			}), 5.0f);
	}
}

void UTapLoginWidget::HideToast()
{
	if (ToastImage)
	{
		ToastImage->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// =============================================================================
// QR 码
// =============================================================================

void UTapLoginWidget::RequestQRCode()
{
	if (!QRService) return;

	LastQRStatus = ETapQRCodeStatus::Pending;
	FString Scopes = FString::Join(ScopeStrings, TEXT(","));
	QRService->RequestQRCode(ClientID, Scopes,
		FOnQRCodeReady::CreateUObject(this, &UTapLoginWidget::OnQRCodeReady));
}

void UTapLoginWidget::OnQRCodeReady(bool bSuccess, const FTapQRCodeData& Data)
{
	if (!bSuccess)
	{
		UE_LOG(LogTapLogin, Error, TEXT("[Panel] QR code request failed"));
		return;
	}

	// 生成 QR 码纹理
	TSharedPtr<FSlateDynamicImageBrush> QRBrush = FTapQRCodeGenerator::GenerateQRBrush(Data.QRCodeUrl, (int32)QRCodeSize);
	UTexture2D* NewTexture = QRBrush.IsValid() ? Cast<UTexture2D>(QRBrush->GetResourceObject()) : nullptr;
	if (NewTexture && QRCodeImage)
	{
		QRCodeTexture = NewTexture;
		QRCodeImage->SetBrush(*QRBrush.Get());
	}

	// 新 QR 码就绪, 恢复显示并隐藏刷新按钮 (toast 由 5 秒计时器自动隐藏)
	if (QRCodeImage) QRCodeImage->SetVisibility(ESlateVisibility::Visible);
	if (RefreshBtn) RefreshBtn->SetVisibility(ESlateVisibility::Collapsed);

	// 开始轮询
	QRService->StartPolling(ClientID,
		FOnQRCodeStatusChanged::CreateUObject(this, &UTapLoginWidget::OnQRCodeStatusChanged),
		FOnQRCodeAuthSuccess::CreateUObject(this, &UTapLoginWidget::OnQRCodeAuthSuccess));
}

void UTapLoginWidget::OnQRCodeStatusChanged(ETapQRCodeStatus Status)
{
	if (Status == LastQRStatus) return; // 状态未变化, 跳过重复处理
	LastQRStatus = Status;

	switch (Status)
	{
	case ETapQRCodeStatus::Pending:
		break;

	case ETapQRCodeStatus::Scanned:
		ShowToast(ToastSuccess);
		break;

	case ETapQRCodeStatus::Denied:
		ShowToast(ToastCancel);
		if (QRCodeImage) QRCodeImage->SetVisibility(ESlateVisibility::Hidden);
		if (RefreshBtn) RefreshBtn->SetVisibility(ESlateVisibility::Visible);
		break;

	case ETapQRCodeStatus::Expired:
		if (QRCodeImage) QRCodeImage->SetVisibility(ESlateVisibility::Hidden);
		if (RefreshBtn) RefreshBtn->SetVisibility(ESlateVisibility::Visible);
		break;

	case ETapQRCodeStatus::Success:
		break;
	}
}

void UTapLoginWidget::OnQRCodeAuthSuccess(const FTapTokenData& TokenData)
{
	UE_LOG(LogTapLogin, Log, TEXT("[Panel] QR auth success"));
	OnAuthSuccess.ExecuteIfBound(TokenData);
}

// =============================================================================
// 按钮事件
// =============================================================================

void UTapLoginWidget::OnCloseClicked()
{
	if (QRService)
	{
		QRService->StopPolling();
	}
	OnClose.ExecuteIfBound();
}

void UTapLoginWidget::OnRefreshClicked()
{
	if (RefreshBtn) RefreshBtn->SetVisibility(ESlateVisibility::Collapsed);
	RequestQRCode();
}

void UTapLoginWidget::OnWebLoginClicked()
{
	ShowToast(ToastLoading);
	OnWebLogin.ExecuteIfBound(ScopeStrings);
}

void UTapLoginWidget::NotifyWebLoginFailed()
{
	ShowToast(ToastFail);
}

void UTapLoginWidget::ShowWebLoginCancelledToast()
{
	ShowToast(ToastCancel);
}

// =============================================================================
// 登录指引浮窗 (iOS/Android 按钮 hover)
// =============================================================================

void UTapLoginWidget::OnIOSBtnHovered()
{
	if (GuideImage && GuideIOSTexture) GuideImage->SetBrushFromTexture(GuideIOSTexture);
	if (GuideArea) GuideArea->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UTapLoginWidget::OnIOSBtnUnhovered()
{
	if (GuideArea) GuideArea->SetVisibility(ESlateVisibility::Collapsed);
}

void UTapLoginWidget::OnAndroidBtnHovered()
{
	if (GuideImage && GuideAndroidTexture) GuideImage->SetBrushFromTexture(GuideAndroidTexture);
	if (GuideArea) GuideArea->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UTapLoginWidget::OnAndroidBtnUnhovered()
{
	if (GuideArea) GuideArea->SetVisibility(ESlateVisibility::Collapsed);
}

#else // Non-desktop platforms: empty stubs for UHT-generated code

UTapLoginWidget::UTapLoginWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UTapLoginWidget::Init(
	const FString& InClientID,
	const TArray<FString>& InScopeStrings,
	FOnLoginPanelClose InOnClose,
	FOnLoginPanelWebLogin InOnWebLogin,
	FOnLoginPanelAuthSuccess InOnAuthSuccess)
{
}

void UTapLoginWidget::NativeConstruct() { Super::NativeConstruct(); }
void UTapLoginWidget::NativeDestruct() { Super::NativeDestruct(); }
void UTapLoginWidget::OnCloseClicked() {}
void UTapLoginWidget::OnRefreshClicked() {}
void UTapLoginWidget::OnWebLoginClicked() {}
void UTapLoginWidget::NotifyWebLoginFailed() {}
void UTapLoginWidget::ShowWebLoginCancelledToast() {}
void UTapLoginWidget::OnIOSBtnHovered() {}
void UTapLoginWidget::OnIOSBtnUnhovered() {}
void UTapLoginWidget::OnAndroidBtnHovered() {}
void UTapLoginWidget::OnAndroidBtnUnhovered() {}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

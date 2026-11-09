// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UTapLoginWidget.generated.h"

// Forward declarations
class UImage;
class UTextBlock;
class UButton;
class UTexture2D;
class UWidgetAnimation;
struct FSlateDynamicImageBrush;

// Desktop: 需要完整类型定义 (UHT 生成的构造函数需要 TUniquePtr 的完整类型)
// Non-desktop: 仅 forward declare 用于委托签名
#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
#include "TapQRCodeService.h"
#else
class FTapQRCodeService;
struct FTapTokenData;
struct FTapQRCodeData;
enum class ETapQRCodeStatus : uint8;
#endif

// 委托声明 (unconditional - delegate template 不需要完整类型定义)
DECLARE_DELEGATE(FOnLoginPanelClose);
DECLARE_DELEGATE_TwoParams(FOnLoginPanelAuthSuccess, const FTapTokenData& /*TokenData*/, bool /*bWebLogin*/);

/**
 * UMG 登录面板 (800x480)
 * 参照 Unity LoginPanel.prefab: 左侧 QR 码扫码 + 右侧网页浏览器登录
 * 使用 BindWidget + Blueprint 模式: C++ 管理业务逻辑, Blueprint 管理布局和样式
 *
 * UI 状态:
 *   1. 初始: QR 码显示, 状态文字隐藏
 *   2. QR 扫码成功: "扫码成功 / 请在手机上确认"
 *   3. QR 被拒: "您已取消此次登录，请重新扫码" + 显示刷新按钮
 *   4. WebBtn: 刷新 QR 码, 用 qrcode_url 打开浏览器, 后台继续轮询 device_code
 *
 * 注: 仅在 Desktop (Win/Mac/Linux) 平台有实际实现,
 *     移动平台提供空实现以满足 UHT 生成代码编译需求
 */
UCLASS()
class TAPLOGIN_API UTapLoginWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UTapLoginWidget(const FObjectInitializer& ObjectInitializer);

	/** 初始化面板参数 (在 AddToViewport 之前调用) */
	void Init(
		const FString& InClientID,
		const TArray<FString>& InScopeStrings,
		FOnLoginPanelClose InOnClose,
		FOnLoginPanelAuthSuccess InOnAuthSuccess);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	// UFUNCTION: UMG 按钮回调 (必须在所有平台声明)
	UFUNCTION() void OnCloseClicked();
	UFUNCTION() void OnWebLoginClicked();
	UFUNCTION() void OnRefreshClicked();
	UFUNCTION() void OnIOSBtnHovered();
	UFUNCTION() void OnIOSBtnUnhovered();
	UFUNCTION() void OnAndroidBtnHovered();
	UFUNCTION() void OnAndroidBtnUnhovered();

	// BindWidgetOptional: Blueprint 中有同名控件时自动绑定，无则为 nullptr
	UPROPERTY(meta = (BindWidgetOptional)) UButton* CloseBtn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* QRCodeImage = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* RefreshBtn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* WebBtn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* IOSBtn = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UButton* AndroidBtn = nullptr;
	// 登录指引浮窗 (iOS/Android 共用, hover 时动态切换图片)
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* GuideArea = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UImage* GuideImage = nullptr;

	// 分割线 (Blueprint 中的 DividerImg / DividerSizeBox, 隐藏不显示)
	UPROPERTY(meta = (BindWidgetOptional)) UImage* DividerImg = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) UWidget* DividerSizeBox = nullptr;

	// QR 码外边框 (去掉可见的边框线)
	UPROPERTY(meta = (BindWidgetOptional)) class UBorder* QROuterBorder = nullptr;
	UPROPERTY(meta = (BindWidgetOptional)) class UBorder* QRInnerBorder = nullptr;

	// 顶部 Toast 通知 (屏幕顶部居中, 直接显示完整 toast 图片)
	UPROPERTY(meta = (BindWidgetOptional)) UImage* ToastImage = nullptr;

	// Toast 纹理 (构造函数中从 Content 加载)
	UPROPERTY() UTexture2D* ToastSuccess = nullptr;
	UPROPERTY() UTexture2D* ToastLoading = nullptr;
	UPROPERTY() UTexture2D* ToastCancel = nullptr;
	UPROPERTY() UTexture2D* ToastFail = nullptr;
	UPROPERTY(Transient) UTexture2D* QRCodeTexture = nullptr;

	// 登录指引纹理 (根据 Region 加载 cn/global 版本)
	UPROPERTY() UTexture2D* GuideIOSTexture = nullptr;
	UPROPERTY() UTexture2D* GuideAndroidTexture = nullptr;

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX
	// QR 码回调
#if WITH_DEV_AUTOMATION_TESTS
	friend struct FTapDesktopLoginTestAccess;
	TFunction<void(const FString&)> OpenBrowserForTest;
	TFunction<TSharedPtr<FSlateDynamicImageBrush>(const FString&, int32)> CreateQRBrushForTest;
#endif
	void RequestQRCode(bool bOpenBrowserOnReady = false);
	void OnQRCodeReady(uint64 RequestId, bool bSuccess, const FTapQRCodeData& Data);
	void OnQRCodeStatusChanged(ETapQRCodeStatus Status);
	void OnQRCodeAuthSuccess(const FTapTokenData& TokenData);

	// 顶部 Toast 通知
	void ShowToast(UTexture2D* ToastTexture);
	void HideToast();
	FTimerHandle ToastTimerHandle;
	FTSTicker::FDelegateHandle ToastTickerHandle;

	// 服务 & 配置
	TSharedPtr<FTapQRCodeService> QRService;
	FString ClientID;
	TArray<FString> ScopeStrings;
	ETapQRCodeStatus LastQRStatus = ETapQRCodeStatus::Pending;
	uint64 QRRequestGeneration = 0;
	bool bQRRequestInFlight = false;
	bool bOpenBrowserWhenQRReady = false;
	bool bWebLoginSession = false;

	// 委托
	FOnLoginPanelClose OnClose;
	FOnLoginPanelAuthSuccess OnAuthSuccess;
#endif
};

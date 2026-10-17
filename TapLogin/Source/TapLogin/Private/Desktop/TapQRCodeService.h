// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

/**
 * QR 码扫码状态
 */
enum class ETapQRCodeStatus : uint8
{
	Pending,     // 等待扫码
	Scanned,     // 已扫码待确认
	Denied,      // 用户拒绝
	Expired,     // 已过期
	Success      // 登录成功
};

/**
 * QR 码设备码数据 (参照 Unity QRCodeData)
 */
struct FTapQRCodeData
{
	FString DeviceCode;
	FString QRCodeUrl;
	int32 ExpiresIn = 0;
	int32 Interval = 5;
};

/**
 * Token 数据 (参照 Unity TokenData)
 */
struct FTapTokenData
{
	FString Kid;
	FString AccessToken;
	FString TokenType;
	FString MacKey;
	FString MacAlgorithm;
	TArray<FString> Scopes;
};

DECLARE_DELEGATE_TwoParams(FOnQRCodeReady, bool /*bSuccess*/, const FTapQRCodeData& /*Data*/);
DECLARE_DELEGATE_OneParam(FOnQRCodeStatusChanged, ETapQRCodeStatus /*Status*/);
DECLARE_DELEGATE_OneParam(FOnQRCodeAuthSuccess, const FTapTokenData& /*TokenData*/);

/**
 * QR 码登录服务
 * 参照 Unity LoginService.cs: POST device/code + 轮询 token
 */
class FTapQRCodeService : public TSharedFromThis<FTapQRCodeService>
{
public:
	FTapQRCodeService();
	~FTapQRCodeService();

	/**
	 * 请求 QR 码 (POST device/code)
	 * @param ClientID TapTap 应用 ClientID
	 * @param Scopes 权限列表 (逗号分隔)
	 * @param OnReady QR 码就绪回调
	 */
	void RequestQRCode(const FString& ClientID, const FString& Scopes,
		FOnQRCodeReady OnReady);

	/**
	 * 开始轮询扫码状态
	 * @param ClientID TapTap 应用 ClientID
	 * @param OnStatusChanged 状态变化回调
	 * @param OnAuthSuccess 认证成功回调
	 */
	void StartPolling(const FString& ClientID,
		FOnQRCodeStatusChanged OnStatusChanged,
		FOnQRCodeAuthSuccess OnAuthSuccess);

	/** 停止轮询 */
	void StopPolling();

	/** 获取当前 QR 码数据 */
	const FTapQRCodeData& GetQRCodeData() const { return QRCodeData; }

private:
	FTapQRCodeData QRCodeData;
	bool bPolling;
	bool bPollRequestInFlight;
	double LastPollTime;
	double QRCodeExpireTime;

	FString PollClientID;
	FOnQRCodeStatusChanged PollStatusCallback;
	FOnQRCodeAuthSuccess PollAuthCallback;
	FTSTicker::FDelegateHandle TickerHandle;

	FString GetWebHost() const;
	FString GetDeviceInfo() const;

	bool PollTick(float DeltaTime);
	void PollOnce();
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

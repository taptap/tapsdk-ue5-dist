// Copyright TapTap. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TapLicenseTypes.generated.h"

/** 对齐 Unity TapLicenseQueryCode */
UENUM(BlueprintType)
enum class ETapLicenseQueryCode : uint8
{
	OK               = 0,   // 查询成功
	NotInstallTapTap = 1,   // 未安装 TapTap 客户端
	Error            = 2,   // 查询失败
	Undefined        = 255,
};

/** 对齐 Unity TapLicensePurchasedCode */
UENUM(BlueprintType)
enum class ETapDLCPurchasedCode : uint8
{
	NotPurchased = 0,  // 未购买 / 退款
	Purchased    = 1,  // 购买成功
	ReturnError  = 2,  // 退款异常
	Undefined    = 255,
};

namespace TapLicenseDLCStatus
{
	inline ETapDLCPurchasedCode FromNativeValue(int32 Status)
	{
		switch (Status)
		{
		case 0:  return ETapDLCPurchasedCode::NotPurchased;
		case 1:  return ETapDLCPurchasedCode::Purchased;
		case -1: return ETapDLCPurchasedCode::ReturnError;
		default: return ETapDLCPurchasedCode::Undefined;
		}
	}

	inline int32 ToNativeValue(ETapDLCPurchasedCode Status)
	{
		return Status == ETapDLCPurchasedCode::ReturnError
			? -1
			: static_cast<int32>(Status);
	}
}

// ── License 回调 ──────────────────────────────────────────────────────────
// 对齐 Unity ITapLicenseCallback
// 触发时机：CheckLicense() 即时结果 + EventId=4001 实时推送
DECLARE_MULTICAST_DELEGATE(FOnLicenseSuccess);
DECLARE_MULTICAST_DELEGATE(FOnLicenseFailed);
/** SDK 调用前置条件或其他 License 错误，不等同于业务上的 LicenseFailed。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnLicenseError, const FString& /*ErrorMessage*/);

// ── DLC 回调 ──────────────────────────────────────────────────────────────
// TMap 含逗号，用 using 别名绕过宏限制
using FTapLicenseDLCResultMap = TMap<FString, bool>;

// 对齐 Unity ITapDlcCallback.OnQueryCallBack
// 触发时机：QueryDLC() 返回结果
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDLCQueryResult,
	ETapLicenseQueryCode           /*Code*/,
	const FTapLicenseDLCResultMap& /*SkuId -> bOwned*/);

// 对齐 Unity ITapDlcCallback.OnOrderCallBack
// 触发时机：EventId=4002 实时推送（购买/退款）
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDLCOrderChanged,
	const FString&       /*SkuId*/,
	ETapDLCPurchasedCode /*Status*/);

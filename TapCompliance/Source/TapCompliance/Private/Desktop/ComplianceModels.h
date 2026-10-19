// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Dom/JsonObject.h"
#include "Internal/Http/TapHttpException.h"

/**
 * Desktop 合规模块所有数据模型
 *
 * 字段命名严格对齐 Unity Standalone 服务端 JSON：
 *   - VerificationResult / LocalVerification     ← /real-name/v1/anti-addiction-token*
 *   - FRealNameConfigResult                       ← /real-name/v1/get-global-config
 *   - FUserAntiConfig + 子结构                    ← /anti-addiction/v1/get-config-by-token
 *   - FPlayableResult                             ← /anti-addiction/v1/heartbeat
 *   - FPayableResult                              ← /anti-addiction/v1/payable
 *   - FComplianceError                            ← 服务端通用错误体
 *
 * 服务端响应统一外层结构 (BaseResponse): {"success":bool,"data":<T>,"now":int64}
 * 失败时附 {"data":{"code":int,"error":str,"error_description":str,"msg":str}}
 */

// =============================================================================
// 错误体 (服务端 data 字段或 errorData 字段)
// =============================================================================

struct FComplianceError
{
	/** TapBasic transport classification; local fallback decisions must not depend on Error strings. */
	ETapHttpErrorType TransportErrorType = ETapHttpErrorType::None;

	int32   ErrorCode = 0;
	FString Error;
	FString ErrorDescription;
	FString Msg;

	/** HTTP 状态码，便于排查（200=业务错误，4xx/5xx=网络层错误） */
	int32   HttpStatus = 0;

	/** 原始响应 body（不超过 1KB），方便测试 / 调试时直接看完整 JSON */
	FString RawResponse;

	bool IsValid() const
	{
		return TransportErrorType != ETapHttpErrorType::None
			|| !Error.IsEmpty()
			|| ErrorCode != 0
			|| HttpStatus != 0;
	}

	/** Token 失效专用判定 (Unity ComplianceException.IsTokenExpired) */
	bool IsTokenExpired() const
	{
		return Error.Equals(TEXT("business_code_error")) && ErrorCode == 200000;
	}

	/** 4xx/business errors must not be hidden by local cache fallback. */
	bool IsRecoverableForCacheFallback() const;

	/** Global config follows Unity Config.Fetch: network/5xx/parse failures may use the last local config. */
	bool IsRecoverableForGlobalConfigCacheFallback() const;

	void FromJson(const TSharedPtr<FJsonObject>& Obj);

	/** 从 TapBasic 异常构造 FComplianceError. */
	static FComplianceError FromException(TSharedPtr<FTapHttpException> E);
};

// =============================================================================
// Verification (服务端 + 本地缓存合并)
// =============================================================================

struct FComplianceVerification
{
	// 服务端字段
	FString Status;             // "pass" / "waiting" / "failed"
	FString ComplianceToken;    // anti_addiction_token

	// 本地补充字段 (持久化)
	FString UserId;
	int32   AgeLimit = 0;       // 默认 0 (对齐 Android IdentityVerifyState.ageLimit=0); 取值 0/8/16/18, 哨兵 100/110
	bool    bIsAdult = false;

	bool IsVerified()     const;
	bool IsWaiting()      const;
	bool IsFailed()       const;
	bool CheckIsAdult()   const;  // age==18 || age==110 (sentinel adult)

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

// =============================================================================
// Global RealName Config
// =============================================================================

struct FCompliancePrompt
{
	FString Title;
	FString Content;             // description_plain
	FString PositiveButtonText;
	FString NegativeButtonText;

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FRealNameText
{
	FCompliancePrompt TapAuthTip;          // taptap_auth
	FCompliancePrompt ManualAuthTip;       // manual_auth
	FCompliancePrompt AuthWaitingTip;      // auth_waiting
	FCompliancePrompt ManualAuthFailedTip; // auth_failed

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FRealNameConfigResult
{
	FRealNameText RealNameText;
	bool          bUseManual = false;      // manual_auth_enable; missing field defaults to Android behavior

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

// =============================================================================
// User Anti-Addiction Config (核心配置)
// =============================================================================

struct FUserState
{
	int32 AgeLimit = 0;    // age_limit (默认 0 对齐 Android UserState.ageLimit=0; 空 real_name:{} → 0)
	bool  bIsAdult = false; // is_adult

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FAntiPolicy
{
	FString Active;                         // policy_active
	FString Model;                          // policy_model
	int32   HeartbeatInterval = 120;        // policy_heartbeat_interval

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FAgeCheckResult
{
	// 默认 false (fail-closed): 对齐 Android ageCheckResult?.allow != true → 拦截.
	// content_rating_check 整体缺失时 FromJson 提前返回, 保留此默认 → 视为不放行.
	bool bAllow = false;                     // allow

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FHealthReminderDesc
{
	FString TipTitle;        // title
	FString TipDescription;  // description_plain

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FHealthReminderTip
{
	FHealthReminderDesc Allow;
	FHealthReminderDesc Reject;

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FTimeRangeConfig
{
	FString             TimeStart;          // time_start ("HH:mm")
	FString             TimeEnd;            // time_end   ("HH:mm")
	FHealthReminderTip  UITipText;          // text
	TArray<FString>     Holidays;           // holidays  ("YYYY-MM-DD")

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FLocalConfig
{
	FTimeRangeConfig TimeRangeConfig;       // time_range

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

struct FUserAntiConfig
{
	FUserState      UserState;              // real_name
	FAntiPolicy     Policy;                 // anti_addiction
	FAgeCheckResult AgeCheckResult;         // content_rating_check
	FLocalConfig    LocalConfig;            // local

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
	TSharedPtr<FJsonObject> ToJson() const;
};

// =============================================================================
// Playable / Payable
// =============================================================================

struct FPlayableResult
{
	int32   RemainTime = 0;                  // remain_time (秒)
	FString Title;                           // title
	FString Content;                         // description_plain

	void FromJson(const TSharedPtr<FJsonObject>& Obj);
};

struct FPayableResult
{
	bool    bAllow = false;                  // allow
	FString Title;                           // title
	FString Content;                         // description_plain

	bool FromJson(const TSharedPtr<FJsonObject>& Obj);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#include "ComplianceModels.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "ComplianceConst.h"
#include "Dom/JsonValue.h"

namespace
{
	FString JStr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, const FString& Default = FString())
	{
		if (!Obj.IsValid()) return Default;
		FString Out;
		return Obj->TryGetStringField(Field, Out) ? Out : Default;
	}

	int32 JInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 Default = 0)
	{
		if (!Obj.IsValid()) return Default;
		int32 Out = 0;
		return Obj->TryGetNumberField(Field, Out) ? Out : Default;
	}

	bool JBool(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, bool Default = false)
	{
		if (!Obj.IsValid()) return Default;
		bool Out = false;
		return Obj->TryGetBoolField(Field, Out) ? Out : Default;
	}

	TSharedPtr<FJsonObject> JObj(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		if (!Obj.IsValid()) return nullptr;
		const TSharedPtr<FJsonObject>* Out = nullptr;
		if (Obj->TryGetObjectField(Field, Out) && Out)
		{
			return *Out;
		}
		return nullptr;
	}

	TArray<FString> JStrArr(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		TArray<FString> Out;
		if (!Obj.IsValid()) return Out;
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (Obj->TryGetArrayField(Field, Arr) && Arr)
		{
			for (const TSharedPtr<FJsonValue>& V : *Arr)
			{
				FString S;
				if (V.IsValid() && V->TryGetString(S))
				{
					Out.Add(S);
				}
			}
		}
		return Out;
	}
}

// =============================================================================
// FComplianceError
// =============================================================================

void FComplianceError::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	TransportErrorType = ETapHttpErrorType::ServerError;
	ErrorCode        = JInt (Obj, TEXT("code"));
	Error            = JStr (Obj, TEXT("error"));
	ErrorDescription = JStr (Obj, TEXT("error_description"));
	Msg              = JStr (Obj, TEXT("msg"));
}

bool FComplianceError::IsRecoverableForCacheFallback() const
{
	if (IsTokenExpired())
	{
		return false;
	}

	if (HttpStatus >= 400 && HttpStatus < 500)
	{
		return false;
	}

	if (HttpStatus >= 500 || ErrorCode >= 500)
	{
		return true;
	}

	return TransportErrorType == ETapHttpErrorType::NetworkError
		|| TransportErrorType == ETapHttpErrorType::Timeout
		|| TransportErrorType == ETapHttpErrorType::InvalidResponse;
}

bool FComplianceError::IsRecoverableForGlobalConfigCacheFallback() const
{
	if (IsTokenExpired())
	{
		return false;
	}

	if (HttpStatus >= 400 && HttpStatus < 500)
	{
		return false;
	}

	if (HttpStatus >= 500 || ErrorCode >= 500)
	{
		return true;
	}

	return TransportErrorType == ETapHttpErrorType::NetworkError
		|| TransportErrorType == ETapHttpErrorType::Timeout
		|| TransportErrorType == ETapHttpErrorType::InvalidResponse;
}

FComplianceError FComplianceError::FromException(TSharedPtr<FTapHttpException> E)
{
	FComplianceError R;
	if (!E.IsValid()) return R;

	R.TransportErrorType = E->GetErrorType();

	if (E->GetErrorType() == ETapHttpErrorType::ServerError)
	{
		auto* Server = static_cast<FTapHttpServerException*>(E.Get());
		const auto& D = Server->GetErrorData();
		R.Error            = D.Error;
		R.ErrorCode        = D.Code;
		R.ErrorDescription = D.ErrorDescription;
		R.Msg              = D.Msg;
		R.HttpStatus       = Server->GetStatusCode();

		// 拼一份等价于原始服务端响应的 JSON 串塞进 RawResponse，方便测试 / 调试看完整 body
		R.RawResponse = FString::Printf(
			TEXT("{\"success\":false,\"now\":%lld,\"data\":{\"code\":%d,\"error\":\"%s\",\"error_description\":\"%s\",\"msg\":\"%s\"}}"),
			Server->GetResponse().Now,
			D.Code, *D.Error, *D.ErrorDescription, *D.Msg);
	}
	else if (E->GetErrorType() == ETapHttpErrorType::InvalidResponse)
	{
		auto* Inv = static_cast<FTapHttpInvalidResponseException*>(E.Get());
		R.ErrorDescription = E->GetMessage();
		R.HttpStatus       = Inv->GetStatusCode();
	}
	else
	{
		R.ErrorDescription = E->GetMessage();
	}
	return R;
}

// =============================================================================
// FComplianceVerification
// =============================================================================

bool FComplianceVerification::IsVerified() const
{
	return Status.Equals(TapComplianceConst::VERIFICATION_STATUS_PASS);
}
bool FComplianceVerification::IsWaiting() const
{
	return Status.Equals(TapComplianceConst::VERIFICATION_STATUS_WAITING);
}
bool FComplianceVerification::IsFailed() const
{
	return Status.Equals(TapComplianceConst::VERIFICATION_STATUS_FAILED);
}
bool FComplianceVerification::CheckIsAdult() const
{
	return AgeLimit == TapComplianceConst::AGE_LIMIT_ADULT
		|| AgeLimit == TapComplianceConst::AGE_LIMIT_SENTINEL_ADULT;
}

void FComplianceVerification::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	Status          = JStr (Obj, TEXT("status"));
	ComplianceToken = JStr (Obj, TEXT("anti_addiction_token"));
	UserId          = JStr (Obj, TEXT("user_id"));
	AgeLimit        = JInt (Obj, TEXT("age_limit"), 0);   // 默认 0 对齐 Android IdentityVerifyState.ageLimit=0
	bIsAdult        = JBool(Obj, TEXT("is_adult"), false);
}

TSharedPtr<FJsonObject> FComplianceVerification::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("status"),               Status);
	Obj->SetStringField(TEXT("anti_addiction_token"), ComplianceToken);
	Obj->SetStringField(TEXT("user_id"),              UserId);
	Obj->SetNumberField(TEXT("age_limit"),            AgeLimit);
	Obj->SetBoolField  (TEXT("is_adult"),             bIsAdult);
	return Obj;
}

// =============================================================================
// FCompliancePrompt / FRealNameText / FRealNameConfigResult
// =============================================================================

void FCompliancePrompt::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	Title              = JStr(Obj, TEXT("title"));
	Content            = JStr(Obj, TEXT("description_plain"));
	NegativeButtonText = JStr(Obj, TEXT("negative_button"));
	PositiveButtonText = JStr(Obj, TEXT("positive_button"));
}

TSharedPtr<FJsonObject> FCompliancePrompt::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("title"),             Title);
	Obj->SetStringField(TEXT("description_plain"), Content);
	Obj->SetStringField(TEXT("negative_button"),   NegativeButtonText);
	Obj->SetStringField(TEXT("positive_button"),   PositiveButtonText);
	return Obj;
}

void FRealNameText::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	TapAuthTip         .FromJson(JObj(Obj, TEXT("taptap_auth")));
	ManualAuthTip      .FromJson(JObj(Obj, TEXT("manual_auth")));
	AuthWaitingTip     .FromJson(JObj(Obj, TEXT("auth_waiting")));
	ManualAuthFailedTip.FromJson(JObj(Obj, TEXT("auth_failed")));
}

TSharedPtr<FJsonObject> FRealNameText::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("taptap_auth"),  TapAuthTip         .ToJson());
	Obj->SetObjectField(TEXT("manual_auth"),  ManualAuthTip      .ToJson());
	Obj->SetObjectField(TEXT("auth_waiting"), AuthWaitingTip     .ToJson());
	Obj->SetObjectField(TEXT("auth_failed"),  ManualAuthFailedTip.ToJson());
	return Obj;
}

void FRealNameConfigResult::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	RealNameText.FromJson(JObj(Obj, TEXT("real_name_text")));
	bUseManual = JBool(Obj, TEXT("manual_auth_enable"), false);
}

TSharedPtr<FJsonObject> FRealNameConfigResult::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("real_name_text"),     RealNameText.ToJson());
	Obj->SetBoolField  (TEXT("manual_auth_enable"), bUseManual);
	return Obj;
}

// =============================================================================
// FUserState / FAntiPolicy / FAgeCheckResult
// =============================================================================

void FUserState::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	AgeLimit = JInt (Obj, TEXT("age_limit"), 0);   // 默认 0 对齐 Android UserState.ageLimit=0 (空 real_name:{} → 0, 不再落哨兵)
	bIsAdult = JBool(Obj, TEXT("is_adult"), false);
}
TSharedPtr<FJsonObject> FUserState::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetNumberField(TEXT("age_limit"), AgeLimit);
	Obj->SetBoolField  (TEXT("is_adult"),  bIsAdult);
	return Obj;
}

void FAntiPolicy::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	Active             = JStr(Obj, TEXT("policy_active"));
	Model              = JStr(Obj, TEXT("policy_model"));
	HeartbeatInterval  = JInt(Obj, TEXT("policy_heartbeat_interval"),
		TapComplianceConst::DEFAULT_HEARTBEAT_INTERVAL_SEC);
	if (HeartbeatInterval <= 0)
	{
		HeartbeatInterval = TapComplianceConst::DEFAULT_HEARTBEAT_INTERVAL_SEC;
	}
}
TSharedPtr<FJsonObject> FAntiPolicy::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("policy_active"),             Active);
	Obj->SetStringField(TEXT("policy_model"),              Model);
	Obj->SetNumberField(TEXT("policy_heartbeat_interval"), HeartbeatInterval);
	return Obj;
}

void FAgeCheckResult::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	// 对齐 Android TapComplianceInternal.kt:506 (ageCheckResult?.allow != true → AGE_LIMIT):
	// allow 缺失 (content_rating_check 为 {}) 视为不放行, 默认 false.
	bAllow = JBool(Obj, TEXT("allow"), false);
}
TSharedPtr<FJsonObject> FAgeCheckResult::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetBoolField(TEXT("allow"), bAllow);
	return Obj;
}

// =============================================================================
// FHealthReminderDesc / FHealthReminderTip / FTimeRangeConfig / FLocalConfig
// =============================================================================

void FHealthReminderDesc::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	TipTitle       = JStr(Obj, TEXT("title"));
	TipDescription = JStr(Obj, TEXT("description_plain"));
}
TSharedPtr<FJsonObject> FHealthReminderDesc::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("title"),             TipTitle);
	Obj->SetStringField(TEXT("description_plain"), TipDescription);
	return Obj;
}

void FHealthReminderTip::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	Allow .FromJson(JObj(Obj, TEXT("allow")));
	Reject.FromJson(JObj(Obj, TEXT("reject")));
}
TSharedPtr<FJsonObject> FHealthReminderTip::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("allow"),  Allow .ToJson());
	Obj->SetObjectField(TEXT("reject"), Reject.ToJson());
	return Obj;
}

void FTimeRangeConfig::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	TimeStart = JStr   (Obj, TEXT("time_start"));
	TimeEnd   = JStr   (Obj, TEXT("time_end"));
	UITipText.FromJson(  JObj(Obj, TEXT("text")));
	Holidays  = JStrArr(Obj, TEXT("holidays"));
}
TSharedPtr<FJsonObject> FTimeRangeConfig::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("time_start"), TimeStart);
	Obj->SetStringField(TEXT("time_end"),   TimeEnd);
	Obj->SetObjectField(TEXT("text"),       UITipText.ToJson());
	TArray<TSharedPtr<FJsonValue>> Arr;
	for (const FString& H : Holidays)
	{
		Arr.Add(MakeShared<FJsonValueString>(H));
	}
	Obj->SetArrayField(TEXT("holidays"), Arr);
	return Obj;
}

void FLocalConfig::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	TimeRangeConfig.FromJson(JObj(Obj, TEXT("time_range")));
}
TSharedPtr<FJsonObject> FLocalConfig::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("time_range"), TimeRangeConfig.ToJson());
	return Obj;
}

// =============================================================================
// FUserAntiConfig
// =============================================================================

void FUserAntiConfig::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	UserState     .FromJson(JObj(Obj, TEXT("real_name")));
	Policy        .FromJson(JObj(Obj, TEXT("anti_addiction")));
	AgeCheckResult.FromJson(JObj(Obj, TEXT("content_rating_check")));
	LocalConfig   .FromJson(JObj(Obj, TEXT("local")));
}
TSharedPtr<FJsonObject> FUserAntiConfig::ToJson() const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetObjectField(TEXT("real_name"),            UserState     .ToJson());
	Obj->SetObjectField(TEXT("anti_addiction"),       Policy        .ToJson());
	Obj->SetObjectField(TEXT("content_rating_check"), AgeCheckResult.ToJson());
	Obj->SetObjectField(TEXT("local"),                LocalConfig   .ToJson());
	return Obj;
}

// =============================================================================
// FPlayableResult / FPayableResult
// =============================================================================

void FPlayableResult::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	if (!Obj.IsValid()) return;
	RemainTime = JInt(Obj, TEXT("remain_time"), 0);
	Title      = JStr(Obj, TEXT("title"));
	Content    = JStr(Obj, TEXT("description_plain"));
}

bool FPayableResult::FromJson(const TSharedPtr<FJsonObject>& Obj)
{
	*this = FPayableResult();
	if (!Obj.IsValid()) return false;
	const bool bHasAllow = Obj->HasField(TEXT("allow"));
	if (bHasAllow && (!Obj->HasTypedField<EJson::Boolean>(TEXT("allow")) ||
		!Obj->TryGetBoolField(TEXT("allow"), bAllow))) return false;
	// Business restriction responses may omit allow and contain only the denial prompt.
	// Keep the default false, but require string fields instead of JSON type coercion.
	if (!bHasAllow && (!Obj->HasTypedField<EJson::String>(TEXT("title")) ||
		!Obj->HasTypedField<EJson::String>(TEXT("description_plain")))) return false;
	const FString ParsedTitle = JStr(Obj, TEXT("title"));
	const FString ParsedContent = JStr(Obj, TEXT("description_plain"));
	if (!bHasAllow && (ParsedTitle.IsEmpty() || ParsedContent.IsEmpty())) return false;
	Title   = ParsedTitle;
	Content = ParsedContent;
	return true;
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#include "Internal/Http/TapHttpErrorConstants.h"

// ============== 通用错误 ==============

const FString FTapHttpErrorConstants::ERROR_INVALID_TIME = TEXT("invalid_time");
const FString FTapHttpErrorConstants::ERROR_SERVER_ERROR = TEXT("server_error");
const FString FTapHttpErrorConstants::ERROR_INVALID_REQUEST = TEXT("invalid_request");
const FString FTapHttpErrorConstants::ERROR_INVALID_CLIENT = TEXT("invalid_client");
const FString FTapHttpErrorConstants::ERROR_INVALID_GRANT = TEXT("invalid_grant");
const FString FTapHttpErrorConstants::ERROR_UNAUTHORIZED_CLIENT = TEXT("unauthorized_client");
const FString FTapHttpErrorConstants::ERROR_ACCESS_DENIED = TEXT("access_denied");
const FString FTapHttpErrorConstants::ERROR_FORBIDDEN = TEXT("forbidden");
const FString FTapHttpErrorConstants::ERROR_NOT_FOUND = TEXT("not_found");
const FString FTapHttpErrorConstants::ERROR_RATE_LIMITED = TEXT("rate_limited");

// ============== 签名相关 ==============

const FString FTapHttpErrorConstants::ERROR_INVALID_SIGN = TEXT("invalid_sign");
const FString FTapHttpErrorConstants::ERROR_SIGN_EXPIRED = TEXT("sign_expired");
const FString FTapHttpErrorConstants::ERROR_REPLAY_ATTACK = TEXT("replay_attack");

// ============== Token 相关 ==============

const FString FTapHttpErrorConstants::ERROR_TOKEN_EXPIRED = TEXT("token_expired");
const FString FTapHttpErrorConstants::ERROR_TOKEN_INVALID = TEXT("token_invalid");
const FString FTapHttpErrorConstants::ERROR_TOKEN_REVOKED = TEXT("token_revoked");

// ============== 验证相关 ==============

const FString FTapHttpErrorConstants::ERROR_CAPTCHA_REQUIRED = TEXT("captcha_required");
const FString FTapHttpErrorConstants::ERROR_CAPTCHA_INVALID = TEXT("captcha_invalid");

// ============== 防沉迷相关 ==============

const FString FTapHttpErrorConstants::ERROR_ANTI_ADDICTION_NOT_VERIFIED = TEXT("anti_addiction_not_verified");
const FString FTapHttpErrorConstants::ERROR_ANTI_ADDICTION_TIME_LIMIT = TEXT("anti_addiction_time_limit");
const FString FTapHttpErrorConstants::ERROR_ANTI_ADDICTION_PAY_LIMIT = TEXT("anti_addiction_pay_limit");

// ============== 用户相关 ==============

const FString FTapHttpErrorConstants::ERROR_USER_NOT_FOUND = TEXT("user_not_found");
const FString FTapHttpErrorConstants::ERROR_USER_EXISTS = TEXT("user_exists");
const FString FTapHttpErrorConstants::ERROR_USER_DISABLED = TEXT("user_disabled");

// ============== 参数相关 ==============

const FString FTapHttpErrorConstants::ERROR_MISSING_PARAMETER = TEXT("missing_parameter");
const FString FTapHttpErrorConstants::ERROR_INVALID_PARAMETER = TEXT("invalid_parameter");

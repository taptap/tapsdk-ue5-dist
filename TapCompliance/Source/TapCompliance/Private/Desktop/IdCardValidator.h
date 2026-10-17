// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

/**
 * 中国大陆居民身份证客户端校验，对齐 Android IdCardUtil.kt：
 *
 * 18 位：长度/全数字前17/省份码/生日(yyyyMMdd)/ISO-7064 校验位 (x/X 通用)
 * 15 位：长度/全数字/省份码/生日(yyMMdd)；无校验位 (Android 同逻辑)
 */
class FIdCardValidator
{
public:
	/** 15 或 18 位中国大陆居民身份证校验 */
	static bool IsValid(const FString& IdCard);

private:
	static bool Validate18(const FString& IdCard);
	static bool Validate15(const FString& IdCard);

	static bool IsAllDigit(const FString& In);
	static bool IsValidProvinceCode(const FString& Prefix2);
	static bool IsValidBirthday8(const FString& Yyyymmdd);   // yyyyMMdd
	static bool IsValidBirthday6(const FString& Yymmdd);     // yyMMdd (15位用)
	/** 计算 18 位校验位 (返回大写: '0'~'9' 或 'X'); 长度不足返回 0 */
	static TCHAR ComputeCheckCode(const FString& First17);
};

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

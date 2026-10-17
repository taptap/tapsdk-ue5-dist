// Copyright TapTap. All Rights Reserved.

#include "IdCardValidator.h"

#if PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

#include "Misc/DateTime.h"

namespace
{
	/** 35 项省份 / 直辖市 / 特别行政区 / 国外代码 (与 Android IdCardUtil 一致) */
	const TCHAR* const GProvinceCodes[] = {
		TEXT("11"), TEXT("12"), TEXT("13"), TEXT("14"), TEXT("15"),
		TEXT("21"), TEXT("22"), TEXT("23"),
		TEXT("31"), TEXT("32"), TEXT("33"), TEXT("34"), TEXT("35"), TEXT("36"), TEXT("37"),
		TEXT("41"), TEXT("42"), TEXT("43"), TEXT("44"), TEXT("45"), TEXT("46"),
		TEXT("50"), TEXT("51"), TEXT("52"), TEXT("53"), TEXT("54"),
		TEXT("61"), TEXT("62"), TEXT("63"), TEXT("64"), TEXT("65"),
		TEXT("71"),
		TEXT("81"), TEXT("82"),
		TEXT("91"),
	};

	/** 加权因子 (前 17 位) */
	constexpr int32 GPower[17] = {
		7, 9, 10, 5, 8, 4, 2, 1, 6, 3, 7, 9, 10, 5, 8, 4, 2
	};

	/** 余数 → 校验位 (索引 = sum % 11) */
	const TCHAR GCheckCodeTable[11] = {
		TEXT('1'), TEXT('0'), TEXT('X'), TEXT('9'), TEXT('8'),
		TEXT('7'), TEXT('6'), TEXT('5'), TEXT('4'), TEXT('3'), TEXT('2')
	};
}

bool FIdCardValidator::IsValid(const FString& IdCard)
{
	if (IdCard.Len() == 15)
	{
		return Validate15(IdCard);
	}
	return Validate18(IdCard);
}

bool FIdCardValidator::Validate18(const FString& IdCard)
{
	if (IdCard.Len() != 18)
	{
		return false;
	}

	const FString First17 = IdCard.Left(17);
	if (!IsAllDigit(First17))
	{
		return false;
	}

	if (!IsValidProvinceCode(IdCard.Left(2)))
	{
		return false;
	}

	if (!IsValidBirthday8(IdCard.Mid(6, 8)))
	{
		return false;
	}

	const TCHAR Expected = ComputeCheckCode(First17);
	if (Expected == 0)
	{
		return false;
	}
	const TCHAR Actual = FChar::ToUpper(IdCard[17]);
	return Actual == Expected;
}

bool FIdCardValidator::Validate15(const FString& IdCard)
{
	if (IdCard.Len() != 15)
	{
		return false;
	}

	if (!IsAllDigit(IdCard))
	{
		return false;
	}

	if (!IsValidProvinceCode(IdCard.Left(2)))
	{
		return false;
	}

	return IsValidBirthday6(IdCard.Mid(6, 6));
}

bool FIdCardValidator::IsAllDigit(const FString& In)
{
	if (In.IsEmpty())
	{
		return false;
	}
	for (const TCHAR Ch : In)
	{
		if (Ch < TEXT('0') || Ch > TEXT('9'))
		{
			return false;
		}
	}
	return true;
}

bool FIdCardValidator::IsValidProvinceCode(const FString& Prefix2)
{
	for (const TCHAR* Code : GProvinceCodes)
	{
		if (Prefix2.Equals(Code))
		{
			return true;
		}
	}
	return false;
}

bool FIdCardValidator::IsValidBirthday6(const FString& Yymmdd)
{
	if (Yymmdd.Len() != 6 || !IsAllDigit(Yymmdd))
	{
		return false;
	}
	// yyMMdd: 年份两位，按 Android SimpleDateFormat("yyMMdd") 规则，
	// 00~99 映射到当前世纪附近 (实际只做月/日合法性即可, 与 Android 对齐)
	const int32 Month = FCString::Atoi(*Yymmdd.Mid(2, 2));
	const int32 Day   = FCString::Atoi(*Yymmdd.Mid(4, 2));
	if (Month < 1 || Month > 12 || Day < 1 || Day > 31)
	{
		return false;
	}
	// 用固定闰年 2000 做 round-trip 日期合法性检查 (与 Android 逻辑等价)
	return FDateTime::Validate(2000, Month, Day, 0, 0, 0, 0);
}

bool FIdCardValidator::IsValidBirthday8(const FString& Yyyymmdd)
{
	if (Yyyymmdd.Len() != 8 || !IsAllDigit(Yyyymmdd))
	{
		return false;
	}
	const int32 Year  = FCString::Atoi(*Yyyymmdd.Left(4));
	const int32 Month = FCString::Atoi(*Yyyymmdd.Mid(4, 2));
	const int32 Day   = FCString::Atoi(*Yyyymmdd.Mid(6, 2));

	// 范围基本约束：Year 1800~9999；Month/Day 由 FDateTime::Validate 兜底
	if (Year < 1800)
	{
		return false;
	}
	if (!FDateTime::Validate(Year, Month, Day, 0, 0, 0, 0))
	{
		return false;
	}

	// 不允许未来出生日期
	const FDateTime Birth(Year, Month, Day);
	if (Birth > FDateTime::UtcNow())
	{
		return false;
	}
	return true;
}

TCHAR FIdCardValidator::ComputeCheckCode(const FString& First17)
{
	if (First17.Len() != 17)
	{
		return 0;
	}
	int32 Sum = 0;
	for (int32 i = 0; i < 17; ++i)
	{
		const TCHAR Ch = First17[i];
		if (Ch < TEXT('0') || Ch > TEXT('9'))
		{
			return 0;
		}
		Sum += (Ch - TEXT('0')) * GPower[i];
	}
	return GCheckCodeTable[Sum % 11];
}

#endif // PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX

// Copyright TapTap. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "TapBasicTypes.generated.h"

/**
 * TapSDK Region Type
 */
UENUM(BlueprintType)
enum class ETapRegion : uint8
{
	CN = 0 UMETA(DisplayName = "China"),
	Global = 1 UMETA(DisplayName = "Global")
};

/**
 * TapSDK Language Type
 */
UENUM(BlueprintType)
enum class ETapLanguage : uint8
{
	Auto = 0 UMETA(DisplayName = "Auto"),            // 自动
	zh_Hans = 1 UMETA(DisplayName = "Simplified Chinese"),   // 简体中文
	en = 2 UMETA(DisplayName = "English"),           // 英文
	zh_Hant = 3 UMETA(DisplayName = "Traditional Chinese"),  // 繁体中文
	ja = 4 UMETA(DisplayName = "Japanese"),          // 日文
	ko = 5 UMETA(DisplayName = "Korean"),            // 韩文
	th = 6 UMETA(DisplayName = "Thai"),              // 泰文
	id = 7 UMETA(DisplayName = "Indonesian"),        // 印度尼西亚语
	de = 8 UMETA(DisplayName = "German"),            // 德语
	es = 9 UMETA(DisplayName = "Spanish"),           // 西班牙语
	fr = 10 UMETA(DisplayName = "French"),           // 法语
	pt = 11 UMETA(DisplayName = "Portuguese"),       // 葡萄牙语
	ru = 12 UMETA(DisplayName = "Russian"),          // 俄罗斯语
	tr = 13 UMETA(DisplayName = "Turkish"),          // 土耳其语
	vi = 14 UMETA(DisplayName = "Vietnamese")        // 越南语
};
